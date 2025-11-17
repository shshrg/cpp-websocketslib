#include "server.h"
#include "config.h"

#include <asio.hpp>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef DEFAULT_CONFIG_WS_PATH
#define DEFAULT_CONFIG_WS_PATH "examples/configs/ws_basic.conf"
#endif

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    std::string config_path = DEFAULT_CONFIG_WS_PATH;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0]
                    << " [--config path/to/config.conf]\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Use --help for usage.\n";
            return 1;
        }
    }

    try {
        std::cout << "Loading config: " << config_path << "\n";
        ServerConfig cfg = load_config(config_path);

        asio::io_context io;

        auto address = asio::ip::make_address(cfg.address);
        bool use_ssl = cfg.use_ssl;


        Server server(io, address, cfg.port, use_ssl);

        if (!cfg.www_root.empty()) {
            server.MountStatic("/", cfg.www_root);
        }
        for (const auto &m: cfg.mounts) {
            server.MountStatic(m.url_prefix, m.root);
        }

        server.WebSocketRouter("/chat")
                .on_open([](const auto &ws) {
                    std::cout << "WebSocket opened\n";
                    ws->send_text_async("Welcome!");
                })
                .on_message([](const auto &ws, std::string_view msg) {
                    std::cout << "WebSocket message received\n";
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
        std::string http_type = (cfg.use_ssl) ? "https" : "http";
        std::cout << "ws example listening on " << http_type << "://" << cfg.address << ":" << cfg.port
                << " (" << threads << " threads)\n";
        std::cout << "Press ENTER to stop...\n";

        std::string dummy;
        std::getline(std::cin, dummy);

        server.stop();
        std::cout << "Server stopped.\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
