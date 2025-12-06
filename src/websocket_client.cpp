#include <asio.hpp>
#include <iostream>
#include <thread>
#include <string>

#include "client.h"

int main(int argc, char* argv[]) {
    std::atomic<bool> connection_alive{true};

    std::string host = "127.0.0.1";
    std::string port = "8080";
    std::string path = "/chat";

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = argv[2];
    if (argc >= 4) path = argv[3];

    try {
        asio::io_context io;

        WebSocketClient client(io);
        std::cout << "[CLIENT] Connecting to ws://" << host << ":" << port << path << "...\n";
        client.connect(host, port, path);
        std::cout << "\n[CLIENT] Connected.\n";

        std::thread recv_thread([&]() {
            try {
                client.receive_loop();  // blocks forever until error/close
            } catch (const std::exception& ex) {
                std::cerr << "\n[CLIENT] Receive loop ended: " << ex.what() << "\n";
            }
            connection_alive = false;
        });

        std::cout << "Type messages to send. Type /quit to exit.\n";
        std::cout << "Commands:\n";
        std::cout << "  /quit   - send close and exit\n";

        std::string line;
        while (connection_alive && std::getline(std::cin, line)) {
            if (line == "/quit") {
                client.send_close(1000, "Client quitting");
                break;
            }
            client.send_text(line);
        }

        if (recv_thread.joinable()) {
            recv_thread.join();
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[CLIENT] Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
