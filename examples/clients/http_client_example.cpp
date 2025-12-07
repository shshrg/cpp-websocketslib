/*
Example of a synchronous HTTP client connecting to server and sending messages
*/

#ifndef DEFAULT_CONFIG_HTTP_PATH
#define DEFAULT_CONFIG_HTTP_PATH "examples/configs/http_basic.conf"
#endif

#include <iostream>
#include <string>
#include "config.h"
#include "http_client_example.h"

int main(int argc, char* argv[]) {
    std::string config_path = DEFAULT_CONFIG_HTTP_PATH;

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
            return 1;
        }
    }

    try {
        ServerConfig cfg = load_config(config_path);

        std::string host = cfg.address.empty() ? "127.0.0.1" : cfg.address;
        std::string port = std::to_string(cfg.port);
        std::string proto = cfg.use_ssl ? "https" : "http";

        std::cout << "Connecting to " << proto << "://" << host << ":" << port << " ...\n";
        HttpClient client(cfg.use_ssl);

#ifdef USE_SSL
        if (cfg.use_ssl && !cfg.ssl_cert_file.empty()) {
            client.set_verify_cert_file(cfg.ssl_cert_file);
        }
#endif

        client.connect(host, port);
        std::cout << "[CLIENT] Connected\n";

        for (;;) {
            if (client.stopped) break;
            std::cout << "> ";
            std::string line;
            if (!std::getline(std::cin, line))
                break;
            if (client.stopped) break;
            if (line.empty()) continue;

            // GET
            if (line.starts_with("get ")) {
                std::string path = line.substr(4);
                std::cout << "[CLIENT] GET " << path << " response:\n"
                          << client.get(path) << "\n";
            }

            // POST
            else if (line.starts_with("post ")) {
                std::string rest = line.substr(5);
                size_t sp = rest.find(' ');

                std::string path;
                std::string body;

                if (sp == std::string::npos) {
                    path = rest;
                    body = "";
                } else {
                    path = rest.substr(0, sp);
                    body = rest.substr(sp + 1);
                }

                std::cout << "[CLIENT] POST " << path << " response:\n"
                          << client.post(path, body) << "\n";
            }

            else if (line == "quit") {
                break;
            }

            else {
                std::cout << "[CLIENT] Unknown command\n";
            }
        }

        std::cout << "[CLIENT] Finished, disconnecting.\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}