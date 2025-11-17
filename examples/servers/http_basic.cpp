#include "server.h"
#include "config.h"

#include <asio.hpp>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "examples/configs/http_basic.conf"
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::string config_path = DEFAULT_CONFIG_PATH;

    // Simple CLI:
    //   ./example_http
    //   ./example_http --config path/to/file.conf
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
        bool use_ssl  = cfg.use_ssl;

        if (use_ssl) {
            std::cerr << "Warning: this is HTTP example, but config enabled SSL.\n";
        }

        Server server(io, address, cfg.port, use_ssl);

        if (!cfg.www_root.empty()) {
            server.MountStatic("/", cfg.www_root);
        }
        for (const auto& m : cfg.mounts) {
            server.MountStatic(m.url_prefix, m.root);
        }

        server.Get("/hello", [](const Request& req) {
            (void)req;
            return Response::text("Hello from http_basic example\n");
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

        std::size_t threads = cfg.threads;
        if (threads == 0) {
            threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
        }

        server.start(threads);
        std::string http_type = (cfg.use_ssl) ? "https" : "http";
        std::cout << "HTTP example listening on " << http_type << "://" << cfg.address << ":" << cfg.port
                  << " (" << threads << " threads)\n";
        std::cout << "Press ENTER to stop...\n";

        std::string dummy;
        std::getline(std::cin, dummy);

        server.stop();
        std::cout << "Server stopped.\n";
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
