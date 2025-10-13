#include "server.h"
#include <iostream>

int main() {
    asio::io_context io_context;

    unsigned short port = 12345;
    Server server(io_context, port);

    server.start();
    std::cout << "Server started on port " << port << "\n";

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}