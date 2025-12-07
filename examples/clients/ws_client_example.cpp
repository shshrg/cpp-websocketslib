/*
Example of a synchronous WebSocket client connecting to server and sending messages
*/

#include "config.h"
#include "websocket_client.h"

#ifndef DEFAULT_CONFIG_WS_PATH
#define DEFAULT_CONFIG_WS_PATH "examples/configs/ws_basic.conf"
#endif

#include <thread>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
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

        std::string host = cfg.address.empty() ? "127.0.0.1" : cfg.address;
        std::string ws_path = "/chat";
        int port = cfg.port;
        std::string proto = cfg.use_ssl ? "wss" : "ws";
        std::cout << "Connecting to " << proto << "://" << host << ":" << port << ws_path << "...\n";

        WebSocketClient client(cfg.use_ssl);
#ifdef USE_SSL
        if (cfg.use_ssl && !cfg.ssl_cert_file.empty()) {
            client.set_verify_cert_file(cfg.ssl_cert_file);
        }
#endif
        client.connect(host, std::to_string(cfg.port), ws_path);

        client.send_text("Hello from client!");

        std::thread reader([&client]() {
            client.receive_loop();
        });

        for (;;) {
            if (client.stopped) break;
            std::string line;
            std::getline(std::cin, line);
            if (client.stopped) break;
            if (line == "quit") break;
            client.send_text(line);
        }

        reader.join();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}