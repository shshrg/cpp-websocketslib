#include <asio.hpp>
#include <asio/ssl.hpp>
#include <iostream>

using asio::ip::tcp;

int main() {
    try {
        asio::io_context io;
        asio::ssl::context ctx(asio::ssl::context::tls_server);
        ctx.use_certificate_chain_file("certs/server.crt");
        ctx.use_private_key_file("certs/server.key", asio::ssl::context::pem);
        ctx.load_verify_file("certs/ca.crt");
        ctx.set_verify_mode(asio::ssl::verify_none);
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 4433));
        tcp::socket socket(io);
        acceptor.accept(socket);
        asio::ssl::stream<tcp::socket> ssl_stream(std::move(socket), ctx);
        ssl_stream.handshake(asio::ssl::stream_base::server);
        std::cout << "Server: handshake complete\n";
        std::array<char, 1024> buf;
        size_t n = ssl_stream.read_some(asio::buffer(buf));
        std::cout << "Received: " << std::string_view(buf.data(), n) << "\n";

        asio::write(ssl_stream, asio::buffer("Message received!\n"));
    } catch (std::exception &e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
}
