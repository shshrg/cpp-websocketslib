// benchmarks/http/bench_myserver_hello.cpp
#include "server.h"
#include <asio.hpp>
#include <iostream>
#include <thread>

int main() {
    try {
        asio::io_context io;

        auto addr = asio::ip::make_address("0.0.0.0");
        unsigned short port = 8080;

        Server server(io, addr, port, /*use_ssl=*/false);

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

        // std::size_t threads = std::max<std::size_t>(
        //         1, std::thread::hardware_concurrency()
        // );
        size_t threads = 4;

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
