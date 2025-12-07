#include "server.h"
#include "config.h"

#include <asio.hpp>
#include <iostream>
#include <string>

int main() {
    try {
        asio::io_context io;

        asio::ip::address address = asio::ip::make_address("0.0.0.0");
        unsigned short port = 9001;
        bool use_ssl = false;

        Server server(io, address, port, use_ssl);

        server.WebSocketRouter("/ws-echo")
            .on_open([](const std::shared_ptr<WebSocket>& ws) {
                std::cout << "[myserver] ws open\n";
            })
            .on_message([](const std::shared_ptr<WebSocket>& ws,
                           std::string_view msg) {
                ws->send_text_async(std::string(msg));
            })
            .on_close([](const std::shared_ptr<WebSocket>&,
                         uint16_t code,
                         std::string_view reason) {
                std::cout << "[myserver] ws close: " << code
                          << " reason=" << reason << "\n";
            });

        std::size_t threads = 1;

        server.start(threads);
        std::cout << "[myserver] WS echo listening on ws://0.0.0.0:"
                  << port << "/ws-echo (" << threads << " threads)\n";
        std::cout << "Press ENTER to stop...\n";

        std::string dummy;
        std::getline(std::cin, dummy);

        server.stop();
        std::cout << "[myserver] stopped\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[myserver] Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
