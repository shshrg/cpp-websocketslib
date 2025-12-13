#ifndef WEBSOCKETLIB_WEBSOCKET_CLIENT_H
#define WEBSOCKETLIB_WEBSOCKET_CLIENT_H

#include <asio.hpp>
#include <memory>
#include <stdexcept>
#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
#include <string>
#include <vector>
#include <atomic>

struct WsInFrame {
    uint8_t opcode = 0;
    bool fin = true;
    std::vector<uint8_t> payload;
};

struct WsMessage {
    uint8_t opcode;
    std::vector<uint8_t> payload;
};


class WebSocketClient {
public:
    explicit WebSocketClient(bool use_ssl = false);
#ifdef USE_SSL
    void set_verify_cert_file(const std::string& file);
#endif
    void connect(const std::string& host, const std::string& port, const std::string& path = "/ws");
    void send_text(const std::string& msg);
    void receive_loop();
    std::atomic<bool> stopped;
    WsInFrame read_frame();
    void send_binary(const std::vector<uint8_t>& data);
    void send_close(uint16_t code);

private:
    asio::io_context internal_io_;
    bool use_ssl_;
#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_;
#endif
    asio::ip::tcp::socket socket_;
    std::string key_;

    static std::string base64_encode(const unsigned char* data, size_t len);
    std::string generate_key();
    std::string read_http_headers();
    void make_frame_text(const std::string& msg, std::vector<uint8_t>& out);

    // void show_jpeg_frame(const uint8_t* data, size_t size);
    WsMessage read_message();

    bool assembling_ = false;
    uint8_t assembling_opcode_ = 0;
    std::vector<uint8_t> assembling_buf_;
};
#endif //WEBSOCKETLIB_WEBSOCKET_CLIENT_H