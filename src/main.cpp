#include "server.h"
#include <iostream>

int main() {
    asio::io_context io_context;

    unsigned short port = 12345;
	auto address = asio::ip::make_address("0.0.0.0");
    Server server(io_context, address, port);

    server.start(8);
    std::cout << "Server started on port " << port << "\n";

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}