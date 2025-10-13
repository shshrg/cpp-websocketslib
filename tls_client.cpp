#include <asio.hpp>
#include <asio/ssl.hpp>
#include <iostream>

using asio::ip::tcp;

int main() {
    try {
        asio::io_context io;
        asio::ssl::context ctx(asio::ssl::context::tls_client);
        ctx.load_verify_file("certs/ca.crt");
        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve("127.0.0.1", "4433");
        asio::ssl::stream<tcp::socket> ssl_stream(io, ctx);
        asio::connect(ssl_stream.next_layer(), endpoints);
        ssl_stream.handshake(asio::ssl::stream_base::client);
        std::cout << "Client: handshake complete\n";
        asio::write(ssl_stream, asio::buffer("Hello Secure Server!\n"));
        std::array<char, 1024> buf;
        size_t n = ssl_stream.read_some(asio::buffer(buf));
        std::cout << "Server says: " << std::string_view(buf.data(), n) << "\n";

    } catch (std::exception &e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }
}
