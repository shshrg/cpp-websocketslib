/*
Example of a synchronous WebSocket client connecting to server and sending messages
*/

#include "config.h"
#include "ws_client.h"
#include "server.h"

#ifndef DEFAULT_CONFIG_WS_PATH
#define DEFAULT_CONFIG_WS_PATH "examples/configs/ws_basic.conf"
#endif

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

        std::cout << "Connecting to ws://" << host << ":" << port << ws_path << "...\n";

        WebSocketClient client;
        client.connect(host, std::to_string(cfg.port), ws_path);

        client.send_text("Hello from client!");

        std::thread reader([&client]() {
            try {
                client.receive_loop();
            } catch (const std::exception& e) {
                std::cout << "Receive loop stopped: " << e.what() << "\n";
            }
        });

        for (;;) {
            std::string line;
            std::getline(std::cin, line);
            if (line == "quit") break;

            client.send_text(line);
        }

        reader.detach();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
