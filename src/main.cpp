#include "server.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <sstream>
#include "config.h"

int main(int argc, char *argv[]) {
    std::string config_path = "examples/configs/http_basic.conf";

    if (argc >= 2) {
        config_path = argv[1];
    }

    try {
        ServerConfig cfg = load_config(config_path);

        asio::io_context io;
        auto address = asio::ip::make_address(cfg.address);

        bool use_ssl = cfg.use_ssl || cfg.ssl_enabled;

        Server server(io, address, cfg.port, use_ssl);

        server.MountStatic("/", cfg.www_root);

        for (const auto &m: cfg.mounts) {
            server.MountStatic(m.url_prefix, m.root);
        }

        server.Get("/hello", [](const Request &request) -> asio::awaitable<Response> {
            co_return Response::text("This was a get method from async");
        });

        server.Post("/hello", [](const Request &request) {
            return Response::text("This was a post method from sync");
        });

        server.Get("/api/files", [](const Request &req) {
            namespace fs = std::filesystem;

            fs::path root = "./www";

            std::vector<std::string> urls;
            std::error_code ec;

            if (!fs::exists(root, ec) || ec) {
                return Response::not_found("www root not found");
            }

            for (auto const &entry: fs::recursive_directory_iterator(root)) {
                if (!entry.is_regular_file()) continue;

                fs::path rel = fs::relative(entry.path(), root, ec);
                if (ec) continue;

                std::string web_path = "/" + rel.generic_string();
                urls.push_back(std::move(web_path));
            }

            std::ostringstream oss;
            oss << "[\n";
            for (size_t i = 0; i < urls.size(); ++i) {
                oss << "  \"";
                for (char c: urls[i]) {
                    if (c == '\"') oss << "\\\"";
                    else oss << c;
                }
                oss << "\"";
                if (i + 1 < urls.size()) oss << ",";
                oss << "\n";
            }
            oss << "]\n";

            Response res = Response::text(oss.str());
            res.set_header("content-type", "application/json");
            return res;
        });

        server.WebSocketRouter("/chat")
                .on_open([](const auto &ws) {
                    std::cout << "WebSocket opened\n";
                    ws->send_text_async("Welcome!");
                })
                .on_message([](const auto &ws, std::string_view msg) {
                    std::cout << "WebSocket message received\n";
                    ws->send_text_async("Echo: " + std::string(msg));
                })
                .on_close([](const auto &ws, uint16_t code, std::string_view reason) {
                    std::cout << "WebSocket closed with " << code
                            << " reason: " << reason << "\n";
                });

        std::size_t threads = cfg.threads;
        if (threads == 0) {
            threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
        }

        server.start(threads);
        std::cout << "Server listening on " << cfg.address << ":" << cfg.port
                << " (threads=" << threads << ", ssl=" << (use_ssl ? "on" : "off") << ")\n";

        auto test_func = []() {
            std::cout << "Task sent using post_task" << std::endl;
        };

        server.post_task(test_func);

        std::string dummy;
        std::getline(std::cin, dummy);
        server.stop();

        std::cout << "Server stopped.\n";
    } catch (const std::exception &ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
