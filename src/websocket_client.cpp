#include "websocket_client.h"
#include <iostream>
#include <random>
#include <array>

#include <cstdint>
#include <chrono>
// #include <opencv2/opencv.hpp>

// inline uint32_t read_u32_be(const uint8_t *p) {
//     return (uint32_t(p[0]) << 24) |
//            (uint32_t(p[1]) << 16) |
//            (uint32_t(p[2]) << 8) |
//            uint32_t(p[3]);
// }
//
// inline uint64_t read_u64_be(const uint8_t *p) {
//     uint64_t v = 0;
//     for (int i = 0; i < 8; ++i) v = (v << 8) | uint64_t(p[i]);
//     return v;
// }
//
// inline uint64_t now_steady_us() {
//     using namespace std::chrono;
//     uint64_t ts_us = duration_cast<microseconds>(
//         system_clock::now().time_since_epoch()
//     ).count();
//
//     return ts_us;
// }

WebSocketClient::WebSocketClient(bool use_ssl)
    : use_ssl_(use_ssl), stopped(false)
#ifdef USE_SSL
      , ssl_ctx_(asio::ssl::context::tls_client)
      , ssl_socket_(internal_io_, ssl_ctx_)
#endif
      , socket_(internal_io_) {
}

#ifdef USE_SSL
void WebSocketClient::set_verify_cert_file(const std::string &file) {
    if (!use_ssl_) return;
    ssl_ctx_.load_verify_file(file);
    ssl_ctx_.set_verify_mode(asio::ssl::verify_peer);
}
#endif

void WebSocketClient::connect(const std::string &host,
                              const std::string &port,
                              const std::string &path) {
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

void WebSocketClient::send_text(const std::string &msg) {
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

void WebSocketClient::receive_loop() {
    for (;;) {
        try {
            WsMessage m = read_message();
            if (m.opcode == 0x1) {
                // text
                std::string msg(reinterpret_cast<char*>(m.payload.data()), m.payload.size());
                std::cout << "[WS CLIENT] Text: " << msg << "\n";
            }
            else if (m.opcode == 0x2) {
                // if (m.payload.size() < 12) {
                //     std::cout << "BAD PACKET: size=" << m.payload.size() << "\n";
                //     continue;
                // }
                //
                // uint32_t frame_id = read_u32_be(m.payload.data());
                // static uint32_t expected = 1;
                // if (frame_id != expected) {
                //     std::cout << "DROP/REORDER: expected " << expected << " got " << frame_id << "\n";
                //     expected = frame_id;
                // }
                // expected++;
                // uint64_t send_ts_us = read_u64_be(m.payload.data() + 4);
                //
                // uint64_t now_us = now_steady_us();
                // uint64_t one_way_us = now_us - send_ts_us;
                //
                // const uint8_t* jpg = m.payload.data() + 12;
                // size_t jpg_size = m.payload.size() - 12;

                // std::cout << "[WS CLIENT] Frame " << frame_id
                //           << " jpeg=" << jpg_size
                //           << " one_way_us=" << one_way_us << "\n";

                // visualize (next section)
                // show_jpeg_frame(jpg, jpg_size);

            }
        } catch (const std::exception &e) {
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

std::string WebSocketClient::base64_encode(const unsigned char *data, size_t len) {
    static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len;) {
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

std::string WebSocketClient::generate_key() {
    std::array<unsigned char, 16> r{};
    std::random_device rd;
    for (auto &b: r)
        b = static_cast<unsigned char>(rd());
    return base64_encode(r.data(), r.size());
}

std::string WebSocketClient::read_http_headers() {
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

void WebSocketClient::make_frame_text(const std::string &msg, std::vector<uint8_t> &out) {
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
    for (auto &b: mask_bytes)
        b = static_cast<uint8_t>(rd());
    out.insert(out.end(), mask_bytes.begin(), mask_bytes.end());

    for (size_t i = 0; i < msg.size(); ++i) {
        auto byte = static_cast<uint8_t>(msg[i]);
        byte ^= mask_bytes[i % 4];
        out.push_back(byte);
    }
}

WsInFrame WebSocketClient::read_frame() {
    static constexpr uint64_t MAX_FRAME_PAYLOAD = 8ull * 1024 * 1024; // 8MB

    uint8_t header[2];
    std::error_code ec;
    auto read_any = [&](void *data, size_t len) {
        if (use_ssl_) {
#ifdef USE_SSL
            asio::read(ssl_socket_, asio::buffer(data, len), ec);
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::read(socket_, asio::buffer(data, len), ec);
        }

        if (ec == asio::error::eof
#ifdef USE_SSL
            || (use_ssl_ && ec == asio::ssl::error::stream_truncated)
#endif
        )
            throw std::runtime_error("Server closed connection");
        if (ec) throw std::runtime_error("Read error: " + ec.message());
    };

    read_any(header, 2);

    WsInFrame f{};

    f.fin = (header[0] & 0x80) != 0;
    f.opcode = header[0] & 0x0F;

    bool masked = header[1] & 0x80;
    uint64_t len = header[1] & 0x7F;
    uint8_t rsv = header[0] & 0x70;
    if (rsv != 0) {
        throw std::runtime_error("RSV bits set (extensions not supported)");
    }

    // std::cout << "[DBG] hdr0=0x" << std::hex << int(header[0])
    //       << " hdr1=0x" << int(header[1]) << std::dec
    //       << " fin=" << f.fin
    //       << " opcode=0x" << std::hex << int(f.opcode) << std::dec
    //       << " masked=" << masked
    //       << " len7=" << len
    //       << "\n";

    switch (f.opcode) {
        case 0x0: case 0x1: case 0x2: case 0x8: case 0x9: case 0xA:
            break;
        default:
            throw std::runtime_error("Invalid opcode: " + std::to_string(f.opcode));
    }

    if (len == 126) {
        uint8_t ext[2];
        read_any(ext, 2);
        len = (static_cast<uint64_t>(ext[0]) << 8) | static_cast<uint64_t>(ext[1]);
    } else if (len == 127) {
        uint8_t ext[8];
        read_any(ext, 8);
        len = 0;
        for (unsigned char i: ext) len = (len << 8) | static_cast<uint64_t>(i);
    }

    std::array<uint8_t, 4> mask_key{0, 0, 0, 0};
    if (masked) {
        read_any(mask_key.data(), 4);
    }

    if (len > MAX_FRAME_PAYLOAD) {
        throw std::runtime_error("Frame too large or parse desync: len=" + std::to_string(len));
    }

    f.payload.resize(len);
    if (len > 0) read_any(f.payload.data(), len);

    if (masked) {
        for (size_t i = 0; i < f.payload.size(); ++i)
            f.payload[i] ^= mask_key[i % 4];
    }

    if (f.opcode == 0x8) throw std::runtime_error("Server closed connection");

    return f;
}


// void WebSocketClient::show_jpeg_frame(const uint8_t* data, size_t size) {
//     cv::Mat buf(1, static_cast<int>(size), CV_8UC1, const_cast<uint8_t*>(data));
//     cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
//     if (img.empty()) return;
//
//     cv::Mat resized_img;
//     double scale_factor = 0.25;
//     cv::resize(img, resized_img, cv::Size(),
//                 scale_factor, scale_factor, cv::INTER_AREA);
//
//     cv::imshow("WS Video", resized_img);
//     cv::waitKey(1);
// }

WsMessage WebSocketClient::read_message() {
    for (;;) {
        WsInFrame f = read_frame(); // low-level: reads ONE frame (maybe fragment)

        // --- Control frames (can be interleaved) ---
        if (f.opcode == 0x8) throw std::runtime_error("Server closed connection");
        if (f.opcode == 0x9) {
            // Ping -> Pong with same payload (optional but recommended)
            // send_pong(f.payload);   // implement or ignore if your server doesn’t ping
            continue;
        }
        if (f.opcode == 0xA) {
            // pong ignore
            continue;
        }

        // --- Data frames: TEXT/BINARY/CONT ---
        if (!assembling_) {
            // Start of a new message
            if (f.opcode == 0x0) {
                // stray continuation; ignore/resync
                continue;
            }
            if (f.opcode != 0x1 && f.opcode != 0x2) {
                throw std::runtime_error("Protocol error: unsupported data opcode");
            }
            assembling_ = true;
            assembling_opcode_ = f.opcode;
            assembling_buf_.clear();
            assembling_buf_.insert(assembling_buf_.end(), f.payload.begin(), f.payload.end());

            if (f.fin) {
                assembling_ = false;
                return WsMessage{assembling_opcode_, std::move(assembling_buf_)};
            }
            continue;
        }
        // Continuation of the current message
        if (f.opcode != 0x0) {
            assembling_ = false;
            assembling_buf_.clear();
            throw std::runtime_error("Protocol error: expected continuation frame");
        }
        assembling_buf_.insert(assembling_buf_.end(), f.payload.begin(), f.payload.end());

        if (f.fin) {
            assembling_ = false;
            return WsMessage{assembling_opcode_, std::move(assembling_buf_)};
        }
    }
}