#include <asio.hpp>
#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
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
#ifdef USE_SSL
using ssl_socket = asio::ssl::stream<tcp::socket>;
#else
using ssl_socket = asio::ip::tcp::socket;
#endif

awaitable<void> do_read(ssl_socket& socket) {
    try {
        // 1) Read one line ending with CRLF
        std::string buffer; // dynamic buffer target
        std::size_t n = co_await asio::async_read_until(
            socket,
            asio::dynamic_buffer(buffer),
            "\r\n\r\n",
            asio::use_awaitable
        );

        // Extract the line without trailing CRLF
        std::string headers = buffer.substr(0, n - 4);
        std::cout << "=== HEADERS ===\n" << headers << "\n";
        // 2) Whatever remains after the delimiter is the beginning of the body
        buffer.erase(0, n);
        if (!buffer.empty()) {
            std::cout << "[BODY CHUNK] " << buffer;
            std::cout.flush();
            buffer.clear();
        }

        // 3) Read body until EOF
        std::array<char, 8192> tmp{};
        for (;;) {
            std::size_t m = co_await socket.async_read_some(asio::buffer(tmp), asio::use_awaitable);
            std::cout.write(tmp.data(), static_cast<std::streamsize>(m));
            std::cout.flush();
        }
    } catch (const std::exception& e) {
        // Will print "End of file" on clean shutdown by the peer
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
            request.target = "/index.html";

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
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", port);

#ifdef USE_SSL
        asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
        ssl_socket socket(io_context, ssl_ctx);
        asio::connect(socket.lowest_layer(), endpoints);
        socket.handshake(asio::ssl::stream_base::client);
#else
        ssl_socket socket(io_context);
        asio::connect(socket, endpoints);
#endif


        co_spawn(io_context, do_read(socket), detached);
        co_spawn(io_context, do_write(socket), detached);

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}