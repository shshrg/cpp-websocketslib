#ifndef WEBSOCKET_CLIENT_SYNC
#define WEBSOCKET_CLIENT_SYNC

#include <asio.hpp>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
#include <iostream>
#include <string>
#include <random>
#include <atomic>

class WebSocketClient {
public:
    explicit WebSocketClient(bool use_ssl = false)
        : use_ssl_(use_ssl), stopped(false)
#ifdef USE_SSL
        , ssl_ctx_(asio::ssl::context::tls_client)
        , ssl_socket_(internal_io_, ssl_ctx_)
#endif
        , socket_(internal_io_)
    {}

#ifdef USE_SSL
    void set_verify_cert_file(const std::string& file) {
        if (!use_ssl_) return;
        ssl_ctx_.load_verify_file(file);
        ssl_ctx_.set_verify_mode(asio::ssl::verify_peer);
    }
#endif

    void connect(const std::string& host,
                 const std::string& port,
                 const std::string& path = "/ws")
    {
        asio::ip::tcp::resolver resolver(internal_io_);
        auto endpoints = resolver.resolve(host, port);

        if (use_ssl_) {
#ifdef USE_SSL
            asio::connect(ssl_socket_.lowest_layer(), endpoints);
            SSL_set_tlsext_host_name(ssl_socket_.native_handle(), host.c_str());
            ssl_socket_.handshake(asio::ssl::stream_base::client);
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::connect(socket_, endpoints);
        }

        key_ = generate_key();

        std::string req =
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + key_ + "\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";

        if (use_ssl_) {
#ifdef USE_SSL
            asio::write(ssl_socket_, asio::buffer(req));
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::write(socket_, asio::buffer(req));
        }

        std::string response = read_http_headers();
        if (response.find("101") == std::string::npos)
            throw std::runtime_error("Handshake failed:\n" + response);
    }

    void send_text(const std::string& msg) {
        if (stopped) return;
        std::vector<uint8_t> frame;
        make_frame_text(msg, frame);

        if (use_ssl_) {
#ifdef USE_SSL
            asio::write(ssl_socket_, asio::buffer(frame));
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::write(socket_, asio::buffer(frame));
        }
    }

    void receive_loop() {
        for (;;) {
            try {
                auto msg = read_frame_text();
                std::cout << "[WS CLIENT] Received: " << msg << "\n";
            } catch (const std::exception& e) {
                std::string what = e.what();
                if (what == "Server closed connection"
                    || what.find("stream truncated") != std::string::npos
                    || what.find("eof") != std::string::npos
                    || what.find("EOF") != std::string::npos
                ) {
                    std::cout << "[WS CLIENT] Connection closed by server." << std::endl;
                } else {
                    std::cerr << "[WS CLIENT] Error: " << what << std::endl;
                }
                stopped = true;
                break;
            }
        }
        stopped = true;
    }

    std::atomic<bool> stopped;

private:
    asio::io_context internal_io_;
    bool use_ssl_;
#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_;
#endif
    asio::ip::tcp::socket socket_;
    std::string key_;

    static std::string base64_encode(const unsigned char* data, size_t len) {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve((len+2)/3*4);

        for (size_t i = 0; i < len; ) {
            uint32_t v =
                (uint32_t(data[i++]) << 16) |
                (i < len ? uint32_t(data[i++]) << 8 : 0) |
                (i < len ? uint32_t(data[i++]) : 0);
            out.push_back(table[(v >> 18) & 0x3F]);
            out.push_back(table[(v >> 12) & 0x3F]);
            out.push_back(i > len + 1 ? '=' : table[(v >> 6) & 0x3F]);
            out.push_back(i > len ? '=' : table[v & 0x3F]);
        }
        return out;
    }

    std::string generate_key() {
        std::array<unsigned char, 16> r{};
        std::random_device rd;
        for (auto& b : r)
            b = static_cast<unsigned char>(rd());
        return base64_encode(r.data(), r.size());
    }

    std::string read_http_headers() {
        asio::streambuf buf;
        std::string out;
        std::string line;
        for (;;) {
            if (use_ssl_) {
#ifdef USE_SSL
                asio::read_until(ssl_socket_, buf, "\r\n");
#else
                throw std::runtime_error("SSL not supported");
#endif
            } else {
                asio::read_until(socket_, buf, "\r\n");
            }
            std::istream is(&buf);
            std::getline(is, line);
            if (line == "\r" || line.empty()) break;
            out += line + "\n";
        }
        return out;
    }

    void make_frame_text(const std::string& msg, std::vector<uint8_t>& out) {
        out.clear();
        uint8_t op = 0x81;
        out.push_back(op);

        size_t len = msg.size();
        uint8_t maskbit = 0x80;

        if (len < 126) {
            out.push_back(maskbit | static_cast<uint8_t>(len));
        } else if (len <= 0xFFFF) {
            out.push_back(maskbit | 126);
            out.push_back((len >> 8) & 0xFF);
            out.push_back(len & 0xFF);
        } else {
            out.push_back(maskbit | 127);
            for (int i = 7; i >= 0; --i)
                out.push_back((len >> (8 * i)) & 0xFF);
        }

        std::array<uint8_t, 4> mask_bytes{};
        std::random_device rd;
        for (auto& b : mask_bytes)
            b = static_cast<uint8_t>(rd());
        out.insert(out.end(), mask_bytes.begin(), mask_bytes.end());

        for (size_t i = 0; i < msg.size(); ++i) {
            auto byte = static_cast<uint8_t>(msg[i]);
            byte ^= mask_bytes[i % 4];
            out.push_back(byte);
        }
    }

    std::string read_frame_text() {
        uint8_t header[2];
        std::error_code ec;
        size_t n = 0;
        if (use_ssl_) {
#ifdef USE_SSL
            n = asio::read(ssl_socket_, asio::buffer(header, 2), ec);
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            n = asio::read(socket_, asio::buffer(header, 2), ec);
        }
        if (ec == asio::error::eof
#ifdef USE_SSL
            || (use_ssl_ && ec == asio::ssl::error::stream_truncated)
#endif
            ) {
            throw std::runtime_error("Server closed connection");
        }
        if (ec) throw std::runtime_error("Read error: " + ec.message());

        bool fin = header[0] & 0x80;
        uint8_t opcode = header[0] & 0x0F;
        bool masked = header[1] & 0x80;
        uint64_t len = header[1] & 0x7F;

        if (!fin) throw std::runtime_error("Fragmentation not supported");
        if (opcode == 0x8) throw std::runtime_error("Server closed connection");
        if (opcode != 0x01) throw std::runtime_error("Only text supported");

        if (len == 126) {
            uint8_t ext[2];
            if (use_ssl_) {
#ifdef USE_SSL
                asio::read(ssl_socket_, asio::buffer(ext, 2));
#else
                throw std::runtime_error("SSL not supported");
#endif
            } else {
                asio::read(socket_, asio::buffer(ext, 2));
            }
            len = (uint64_t(ext[0]) << 8) | uint64_t(ext[1]);
        } else if (len == 127) {
            uint8_t ext[8];
            if (use_ssl_) {
#ifdef USE_SSL
                asio::read(ssl_socket_, asio::buffer(ext, 8));
#else
                throw std::runtime_error("SSL not supported");
#endif
            } else {
                asio::read(socket_, asio::buffer(ext, 8));
            }
            len = 0;
            for (int i = 0; i < 8; ++i)
                len = (len << 8) | uint64_t(ext[i]);
        }

        std::array<uint8_t, 4> mask_key{0, 0, 0, 0};
        if (masked) {
            if (use_ssl_) {
#ifdef USE_SSL
                asio::read(ssl_socket_, asio::buffer(mask_key, 4));
#else
                throw std::runtime_error("SSL not supported");
#endif
            } else {
                asio::read(socket_, asio::buffer(mask_key, 4));
            }
        }

        std::string msg(len, 0);
        if (use_ssl_) {
#ifdef USE_SSL
            asio::read(ssl_socket_, asio::buffer(msg.data(), len));
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::read(socket_, asio::buffer(msg.data(), len));
        }

        if (masked) {
            for (size_t i = 0; i < len; i++)
                msg[i] ^= mask_key[i % 4];
        }

        return msg;
    }
};

#endif // WEBSOCKET_CLIENT_SYNC