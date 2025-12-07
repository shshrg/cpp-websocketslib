#ifndef WEBSOCKET_CLIENT_SYNC
#define WEBSOCKET_CLIENT_SYNC

#include <asio.hpp>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <string>
#include <random>
#include "hash/sha1_wrapper.h"
#include "websocket/WsFrame.h"
#include "http/request.h"


class WebSocketClient{
public:
    WebSocketClient(asio::io_context &context)
        : io(context), socket(context)
        {}
    
    void connect(const std::string& host,
                                  const std::string& port,
                                  const std::string& path = "/ws") {
        asio::ip::tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(host, port);

        asio::connect(socket, endpoints);
        key = generate_key();

        std::string req = 
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + key + "\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        
        asio::write(socket, asio::buffer(req));
        
        std::string response = read_http_headers();
        if (response.find("101") == std::string::npos)
            throw std::runtime_error("Handshake failed:\n" + response);

        std::cout << "[CLIENT] Handshake successful";
    }

    void send_text(const std::string& msg) {
        std::vector<uint8_t> frame;
        make_frame_text(msg, frame);
        asio::write(socket, asio::buffer(frame));
    }

    void receive_loop() {
        for (;;) {
            auto msg = read_frame_text();
        }
    }

    void send_close(uint16_t code = 1000, const std::string& reason = "")
    {
        std::string payload;
        payload.push_back(static_cast<char>((code >> 8) & 0xFF));
        payload.push_back(static_cast<char>(code & 0xFF));
        payload += reason;

        std::vector<uint8_t> frame;

        uint8_t op = 0x88;
        frame.push_back(op);

        uint8_t maskbit = 0x80;
        size_t len = payload.size();

        if (len < 126) {
            frame.push_back(maskbit | uint8_t(len));
        } else {
            throw std::runtime_error("Close reason too long");
        }

        uint32_t mask = 0x11223344;
        uint8_t* m = reinterpret_cast<uint8_t*>(&mask);

        // Write mask bytes
        frame.push_back(m[0]);
        frame.push_back(m[1]);
        frame.push_back(m[2]);
        frame.push_back(m[3]);

        for (size_t i = 0; i < payload.size(); i++)
            frame.push_back(payload[i] ^ m[i % 4]);

        asio::write(socket, asio::buffer(frame));
    }
private:
    asio::io_context& io;
    asio::ip::tcp::socket socket;
    std::string key;

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

        while (true) {
            asio::read_until(socket, buf, "\r\n");
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
        for (auto& b : mask_bytes) {
            b = static_cast<uint8_t>(rd());
        }

        out.insert(out.end(), mask_bytes.begin(), mask_bytes.end());

        for (size_t i = 0; i < msg.size(); ++i) {
            auto byte = static_cast<uint8_t>(msg[i]);
            byte ^= mask_bytes[i % 4];
            out.push_back(byte);
        }
    }


    std::string read_frame_text() {
        uint8_t header[2];
        asio::read (socket, asio::buffer(header, 2));

        bool fin = header[0] & 0x80;
        uint8_t opcode = header[0] & 0x0F;
        bool masked = header[1] & 0x80;
        uint64_t len = header[1] & 0x7F;

        
        if (!fin) throw std::runtime_error("Fragmentation not supported");
        if (opcode == 0x8) {
            std::string payload(len, 0);
            if (len > 0) {
                asio::read(socket, asio::buffer(payload.data(), len));
            }
            socket.close();
            throw std::runtime_error("Connection closed by server");
        }
        if (opcode != 0x01) throw std::runtime_error("Only text supported");

        std::cout << "Length of message received : " << len << "\n";
        if (len == 126) {
            std::array<uint8_t,2> ext{};
            asio::read(socket, asio::buffer(ext, 2));
            len = (static_cast<uint64_t>(ext[0]) << 8) | static_cast<uint64_t>(ext[1]);
        } else if (len == 127) {
            uint8_t ext[8];
            asio::read(socket, asio::buffer(ext, 8));
            len = 0;
            for (int i = 0; i < 8; i++)
                len = (len << 8) | static_cast<uint64_t>(ext[i]);
        }

        std::array<uint8_t,4> mask_key{0,0,0,0};
        if (masked)
            asio::read(socket, asio::buffer(mask_key, 4));

        std::string msg(len, 0);
        asio::read(socket, asio::buffer(msg.data(), len));

        if (masked)
            for (size_t i = 0; i < len; i++)
                msg[i] ^= mask_key[i%4];
        
        return msg;
    }

};




#endif