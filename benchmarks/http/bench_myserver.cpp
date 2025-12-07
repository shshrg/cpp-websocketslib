// benchmarks/http/bench_myserver_hello.cpp
#include "server.h"
#include <asio.hpp>
#include <iostream>
#include <thread>
#include "config.h"

#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "examples/configs/http_basic.conf"
#endif

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    std::string config_path = DEFAULT_CONFIG_PATH;

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

        auto addr = asio::ip::make_address(cfg.address);
        bool use_ssl  = cfg.use_ssl;

        if (use_ssl) {
            std::cerr << "Warning: this is HTTP example, but config enabled SSL.\n";
        }

        Server server(io, addr, cfg.port, use_ssl);

        server.Get("/hello", [](const Request &req) {
            (void) req;
            return Response::text("Hello benchmark\n");
        });

        server.MountStatic("/static", "./www/static");

        // Important: add Connection: close unless you implemented keep-alive
        server.Get("/ping", [](const Request &) {
            Response r = Response::text("OK\n");
            r.set_header("Connection", "close");
            return r;
        });

        server.Post("/echo", [](const Request &req) -> Response {
            Response r = Response::text(req.body); // Echo body
            r.set_header("content-type", "application/octet-stream");
            // Optional: enable or disable keep-alive here
            return r;
        });

        size_t threads = cfg.threads;

        if (threads == 0) {
            threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
        }

        server.start(threads);

        std::cout << "My server benchmark listening on http://0.0.0.0:8080/hello"
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