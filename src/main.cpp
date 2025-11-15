#include "server.h"
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
    unsigned short port = 8080;
    size_t num_threads = 8;
    bool use_ssl = false;

    // port
    if (argc >= 2) {
        try {
            port = static_cast<unsigned short>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    // threads
    if (argc >= 3) {
        try {
            num_threads = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid thread count: " << argv[2] << "\n";
            return 1;
        }
    }

    // ssl
    if (argc >= 4) {
        std::string arg = argv[3];
        if (arg == "ssl") {
            use_ssl = true;
        } else {
            std::cerr << "Invalid argument: " << arg << "\n";
            std::cerr << "Usage: " << argv[0] << " [port] [threads] [ssl]\n";
            return 1;
        }
    }

    asio::io_context io_context;
    auto address = asio::ip::make_address("0.0.0.0");

    Server server(io_context, address, port, use_ssl);

    server.Get("/hello", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a get method from async");
    });

    server.Post("/hello", [](const Request &request) {
        return Response::text("This was a post method from sync");
    });

    server.MountStatic("/", "./www");
    server.MountStatic("/assets/", "./www/assets");
    server.MountStatic("/assets/ui", "./www/assets/ui");

    server.WebSocket("/chat")
            .on_open([] {
                std::cout << "WebSocket opened\n";
            })
            .on_message([](std::string_view msg) {
                std::cout << "WebSocket message received\n";
            })
            .on_close([](uint16_t code, std::string_view reason) {
                std::cout << "WebSocket closed\n";
            });

    server.start(num_threads);
    std::cout << "Server started on port " << port << "\n";
    auto test_func = []() {
        std::cout << "Task sent using post_task" << std::endl;
    };

    server.post_task(test_func);

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}
