#include "server.h"
#include <iostream>

int main(int argc, char *argv[]) {
    unsigned short port = 12345;
    size_t num_threads = 8;
    if (argc >= 2) {
        try {
            port = static_cast<unsigned short>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    if (argc >= 3) {
        try {
            num_threads = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid thread count: " << argv[2] << "\n";
            return 1;
        }
    }

    if (argc > 3) {
        std::cerr << "Usage:\n  " << argv[0] << " [port] [threads]\n";
        return 1;
    }

    asio::io_context io_context;

	auto address = asio::ip::make_address("0.0.0.0");
    Server server(io_context, address, port, true);

    server.Get("/hello", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("OK");
    });

    server.start(num_threads);
    std::cout << "Server started on port " << port << "\n";

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}