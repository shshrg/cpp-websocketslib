#include <asio.hpp>
#include <asio/ssl.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <string>
#include "http/request.h"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using namespace asio::experimental::awaitable_operators;
using ssl_socket = asio::ssl::stream<tcp::socket>;

awaitable<void> do_read(ssl_socket &socket) {
    try {
        std::vector<char> buf(1024);
        while (true) {
            std::size_t n = co_await socket.async_read_some(asio::buffer(buf), asio::use_awaitable);
            std::string data(buf.data(), n);
            std::cout << "Server replied: " << data << "\n";
        }
    } catch (std::exception &e) {
        std::cout << "Read stopped: " << e.what() << "\n";
    }
}

awaitable<void> do_write(ssl_socket &socket) {
    try {
        while (true) {
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) break;

            Request request;
            request.method = Method::GET;
            request.version = "HTTP/1.1";
            request.target = "/hello";

            request.headers.emplace("Host", "localhost");
            request.headers.emplace("Content-Length", std::to_string(line.size()));
            request.headers.emplace("Content-Type", "text/plain; charset=utf-8");

            request.body = line;
            std::string req_str = request.to_string();
            // std::cout << "Sending request:\n" << req_str << std::endl;
            co_await asio::async_write(socket, asio::buffer(req_str), asio::use_awaitable);
        }

        socket.lowest_layer().close();
    } catch (std::exception &e) {
        std::cout << "Write stopped: " << e.what() << "\n";
    }
}

int main(int argc, char *argv[]) {
    try {
        auto port = "12345";
        if (argc >= 2) {
            port = argv[1];
        }

        if (argc > 2) {
            std::cerr << "Usage:\n  " << argv[0] << " [port]\n";
            return 1;
        }
        asio::io_context io_context;

        asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
        // ssl_ctx.set_verify_mode(asio::ssl::verify_none);

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", port);
        // tcp::socket socket(io_context);
        // asio::connect(socket, endpoints);
        // std::cout << "Connected to server.\n";

        ssl_socket socket(io_context, ssl_ctx);
        asio::connect(socket.lowest_layer(), endpoints);
        socket.handshake(asio::ssl::stream_base::client);
        std::cout << "Connected securely to server.\n";

        co_spawn(io_context, do_read(socket), detached);
        co_spawn(io_context, do_write(socket), detached);

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}