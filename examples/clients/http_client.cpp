#include "config.h"
#include "http_client.h"

#ifndef DEFAULT_CONFIG_HTTP_PATH
#define DEFAULT_CONFIG_HTTP_PATH "examples/configs/http_basic.conf"
#endif

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
        client.connect(host, port);
        std::cout << "[CLIENT] Connected\n";

        std::cout << "> ";
        std::string line;
        if (std::getline(std::cin, line)) {
            if (line.starts_with("get ")) {
                std::string path = line.substr(4);
                try {
                    std::cout << "[CLIENT] GET " << path << " response:\n"
                              << client.get(path) << "\n";
                } catch (const std::exception& e) {
                    std::cout << "[CLIENT] HTTP GET error: " << e.what() << "\n";
                }
            } else if (line.starts_with("post ")) {
                size_t sp = line.find(' ', 5);
                if (sp != std::string::npos) {
                    std::string path = line.substr(5, sp - 5);
                    std::string body = line.substr(sp + 1);
                    try {
                        std::cout << "[CLIENT] POST " << path << " response:\n"
                                  << client.post(path, body) << "\n";
                    } catch (const std::exception& e) {
                        std::cout << "[CLIENT] HTTP POST error: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "[CLIENT] Invalid POST command.\n";
                }
            } else {
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
