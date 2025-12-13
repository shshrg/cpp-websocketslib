#include "server.h"
#include "config.h"

#include <asio.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <map>
#include <mutex>

struct SessionState {
    std::mutex mtx;
    std::string category = "winter"; // Default video
    uint32_t frame_id = 0;
};

std::map<size_t, std::shared_ptr<SessionState> > g_sessions;
std::mutex g_sessions_mtx;

static void append_u32_be(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back(uint8_t((v >> 24) & 0xFF));
    out.push_back(uint8_t((v >> 16) & 0xFF));
    out.push_back(uint8_t((v >> 8) & 0xFF));
    out.push_back(uint8_t(v & 0xFF));
}

static void append_u64_be(std::vector<uint8_t> &out, uint64_t v) {
    for (int i = 7; i >= 0; --i) out.push_back(uint8_t((v >> (8 * i)) & 0xFF));
}

static bool read_file_bytes(const std::string &path, std::vector<uint8_t> &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t n = size_t(f.tellg());
    f.seekg(0, std::ios::beg);
    out.resize(n);
    f.read(reinterpret_cast<char *>(out.data()), out.size());
    return true;
}

static std::string fmt6(uint32_t x) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%06u.jpg", x);
    return std::string(buf);
}


int main() {
    try {
        asio::io_context io;

        asio::ip::address address = asio::ip::make_address("127.0.0.1");
        unsigned short port = 9001;
        bool use_ssl = false;

        Server server(io, address, port, use_ssl);

        server.WebSocketRouter("/ws-echo")
                .on_open([](const std::shared_ptr<WebSocket> &ws) {
                    std::cout << "[myserver] ws open\n";
                })
                .on_message([](const std::shared_ptr<WebSocket> &ws,
                               std::string_view msg) {
                    ws->send_text_async(std::string(msg));
                })
                .on_close([](const std::shared_ptr<WebSocket> &,
                             uint16_t code,
                             std::string_view reason) {
                    std::cout << "[myserver] ws close: " << code
                            << " reason=" << reason << "\n";
                });


        server.WebSocketRouter("/ws-video")
                .on_open([&](std::shared_ptr<WebSocket> ws) {
                    // Streamer coroutine: paced 30 FPS on the WS executor
                    auto state = std::make_shared<SessionState>();

                    {
                        std::lock_guard<std::mutex> lock(g_sessions_mtx);
                        g_sessions[(size_t) ws.get()] = state;
                    }

                    asio::co_spawn(
                        ws->get_executor(),
                        [ws, state]() -> asio::awaitable<void> {
                            using namespace std::chrono;
                            asio::steady_timer timer(co_await asio::this_coro::executor);
                            const auto frame_interval = 33ms; // ~30 FPS


                            for (;;) {
                                if (!ws->is_open()) co_return;

                                std::string current_cat;
                                uint32_t current_id;

                                {
                                    std::lock_guard<std::mutex> lock(state->mtx);
                                    current_cat = state->category;
                                    current_id = ++state->frame_id;
                                }
                                std::string path = "frames/" + current_cat + "/" + fmt6(current_id);
                                std::vector<uint8_t> jpg;
                                if (!read_file_bytes(path, jpg)) {
                                    {
                                        std::lock_guard<std::mutex> lock(state->mtx);
                                        state->frame_id = 0;
                                    }

                                    // prevent busy loop if frames are missing
                                    timer.expires_after(frame_interval);
                                    co_await timer.async_wait(asio::use_awaitable);

                                    continue;
                                }

                                uint64_t ts_us = duration_cast<microseconds>(
                                    system_clock::now().time_since_epoch()
                                ).count();


                                std::vector<uint8_t> packet;
                                packet.reserve(4 + 8 + jpg.size());
                                append_u32_be(packet, current_id);
                                append_u64_be(packet, ts_us);
                                packet.insert(packet.end(), jpg.begin(), jpg.end());
                                ws->send_binary_async(std::move(packet));

                                timer.expires_after(frame_interval);
                                co_await timer.async_wait(asio::use_awaitable);
                            }
                        },
                        asio::detached
                    );
                })
                .on_message([&](std::shared_ptr<WebSocket> ws, std::string_view msg) {
                    std::string cmd(msg);

                    std::cout << "[server] Switch video request: " << cmd << "\n";

                    std::shared_ptr<SessionState> state;
                    {
                        std::lock_guard<std::mutex> lock(g_sessions_mtx);
                        auto it = g_sessions.find((size_t) ws.get());
                        if (it != g_sessions.end()) state = it->second;
                    }

                    if (state) {
                        std::lock_guard<std::mutex> lock(state->mtx);
                        if (cmd == "winter" || cmd == "cat" || cmd == "car") {
                            state->category = cmd;
                            state->frame_id = 0; // Reset video to start
                        }
                    }
                })
                .on_close([&](std::shared_ptr<WebSocket> ws, uint16_t, std::string_view) {
                    // 5. Cleanup
                    std::lock_guard<std::mutex> lock(g_sessions_mtx);
                    g_sessions.erase((size_t) ws.get());
                });

        server.MountStatic("/videos", "./www");

        std::size_t threads = 2;

        server.start(threads);
        std::cout << "[myserver] WS echo listening on ws://127.0.0.1:"
                << port << "/ws-echo (" << threads << " threads)\n";
        std::cout << "Press ENTER to stop...\n";

        std::string dummy;
        std::getline(std::cin, dummy);

        server.stop();
        std::cout << "[myserver] stopped\n";

        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "[myserver] Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
