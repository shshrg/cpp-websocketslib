/**
 * @file websocket_client.h
 * @brief A simple WebSocket client with optional SSL/TLS support.
 *
 * This file defines the `WebSocketClient` class, which allows connecting
 * to WebSocket servers, sending text messages, receiving messages (text or binary),
 * and handling fragmented frames and control frames (Ping/Pong/Close).
 *
 * Features:
 * - Support for ws:// and wss:// (SSL/TLS) connections
 * - WebSocket handshake and frame encoding/decoding
 * - Automatic message assembly from fragmented frames
 * - Optional SSL certificate verification
 *
 * Dependencies:
 * - Asio standalone or Boost.Asio
 * - Optional: OpenSSL for SSL/TLS
 */

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

/**
 * @brief Represents a single incoming WebSocket frame.
 */
struct WsInFrame {
    uint8_t opcode = 0;             ///< WebSocket opcode (text, binary, close, ping, pong)
    bool fin = true;                ///< FIN bit: true if this is the final fragment
    std::vector<uint8_t> payload;   ///< Frame payload data
};

/**
 * @brief Represents a complete WebSocket message (assembled from frames).
 */
struct WsMessage {
    uint8_t opcode;                 ///< Message type (text=0x1, binary=0x2)
    std::vector<uint8_t> payload;   ///< Message payload
};

/**
 * @class WebSocketClient
 * @brief Simple WebSocket client class with optional SSL/TLS support.
 *
 * Allows connecting to WebSocket servers, sending text messages,
 * receiving messages, and handling fragmented frames and control frames.
 */
class WebSocketClient {
public:
    /**
     * @brief Constructs a WebSocket client.
     * @param use_ssl True to enable SSL/TLS (requires USE_SSL compilation flag)
     */
    explicit WebSocketClient(bool use_ssl = false);

#ifdef USE_SSL
    /**
     * @brief Sets SSL certificate verification file.
     * @param file Path to certificate file
     */
    void set_verify_cert_file(const std::string& file);
#endif

    /**
     * @brief Connects to a WebSocket server and performs handshake.
     * @param host Server hostname or IP
     * @param port Server port
     * @param path WebSocket endpoint path (default: "/ws")
     * @throws std::runtime_error on failure
     */
    void connect(const std::string& host, const std::string& port, const std::string& path = "/ws");

    /**
     * @brief Sends a text message to the WebSocket server.
     * @param msg Text message
     */
    void send_text(const std::string& msg);

    /**
     * @brief Receives messages in a loop and prints them to stdout.
     *        This function blocks until an error occurs or the connection is closed.
     */
    void receive_loop();

    /**
     * @brief Indicates if the client has stopped or the connection closed.
     */
    std::atomic<bool> stopped;

    /**
     * @brief Reads a single WebSocket frame from the server.
     * @return WsInFrame A single frame
     * @throws std::runtime_error on protocol errors or disconnect
     */
    WsInFrame read_frame();

private:
    asio::io_context internal_io_;   ///< Internal ASIO IO context
    bool use_ssl_;                   ///< True if SSL/TLS is enabled
#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;                                     ///< SSL context
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_;           ///< SSL socket
#endif
    asio::ip::tcp::socket socket_;  ///< Plain TCP socket
    std::string key_;                ///< WebSocket handshake key

    /**
     * @brief Encodes data into base64 (used for WebSocket handshake key).
     * @param data Input data
     * @param len Data length
     * @return Base64-encoded string
     */
    static std::string base64_encode(const unsigned char* data, size_t len);

    /**
     * @brief Generates a random WebSocket key for the handshake.
     * @return Generated key
     */
    std::string generate_key();

    /**
     * @brief Reads HTTP headers from the server during handshake.
     * @return Concatenated header string
     */
    std::string read_http_headers();

    /**
     * @brief Encodes a text message into a WebSocket frame with masking.
     * @param msg Input text message
     * @param out Output frame bytes
     */
    void make_frame_text(const std::string& msg, std::vector<uint8_t>& out);

    // void show_jpeg_frame(const uint8_t* data, size_t size); ///< Optional OpenCV visualization

    /**
     * @brief Reads a complete WebSocket message (assembles fragmented frames).
     * @return WsMessage The assembled message
     */
    WsMessage read_message();

    bool assembling_ = false;               ///< True if currently assembling a fragmented message
    uint8_t assembling_opcode_ = 0;        ///< Opcode of the message being assembled
    std::vector<uint8_t> assembling_buf_;  ///< Buffer storing the fragments
};

#endif // WEBSOCKETLIB_WEBSOCKET_CLIENT_H
