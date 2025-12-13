#ifndef WEB_SOCKET_H
#define WEB_SOCKET_H

#include <asio.hpp>
#include <memory>
#include <atomic>
#include "ws_helpers.h"
#include "WsFrame.h"
#include "http/response.h"
#include "http/utils.h"
#include <variant>
#include <deque>

/**
 * @file WebSocket.h
 * @brief Asynchronous WebSocket connection class using ASIO.
 *
 * This header defines:
 * - `WebSocket`: a class representing a single WebSocket connection.
 *   It handles reading/writing frames asynchronously, including text,
 *   binary, ping/pong, and close frames.
 * - `WsHandlers`: struct for user-provided event callbacks (on_open,
 *   on_message, on_close).
 * - `WsOpenHandler`, `WsMessageHandler`, `WsCloseHandler`: function
 *   types for event callbacks.
 * - WebSocket connection state constants (`WS_OPEN`, `WS_CLOSED`).
 * - Type aliases for TCP/SSL sockets and a variant socket type.
 *
 * Features:
 * - Thread-safe serialized writes via an ASIO strand.
 * - Fragmented frame assembly for text/binary messages.
 * - Optional SSL/TLS support.
 * - Coroutine-based async read/write operations.
 */

class WebSocket;

/// Handler type for WebSocket open events
using WsOpenHandler = std::function<void(std::shared_ptr<WebSocket>)>;

/// Handler type for WebSocket message events (text or binary)
using WsMessageHandler = std::function<void(std::shared_ptr<WebSocket>, std::string_view msg)>;

/// Handler type for WebSocket close events
using WsCloseHandler = std::function<void(std::shared_ptr<WebSocket>, uint16_t code, std::string_view reason)>;

/**
 * @brief Struct holding user-provided WebSocket event handlers.
 */
struct WsHandlers {
    WsOpenHandler on_open{};       ///< Called when the WebSocket connection opens
    WsMessageHandler on_message{}; ///< Called when a full text or binary message is received
    WsCloseHandler on_close{};     ///< Called when the WebSocket connection closes
};

/// Connection state flags
enum: uint8_t {
    WS_OPEN = 0x0,    ///< WebSocket is open
    WS_CLOSED = 0x1   ///< WebSocket is closed
};

using TcpSocket = asio::basic_stream_socket<asio::ip::tcp>;

#ifdef USE_SSL
#include <asio/ssl.hpp>
using SslSocket = asio::ssl::stream<TcpSocket>; ///< SSL/TLS wrapped socket
using AnySocket = std::variant<TcpSocket, SslSocket>; ///< Can store either TCP or SSL socket
#else
using AnySocket = std::variant<TcpSocket>; ///< Only TCP socket
#endif

/**
 * @brief WebSocket connection abstraction.
 *
 * Handles reading and writing frames asynchronously using ASIO coroutines.
 * Provides thread-safe write serialization using an ASIO strand.
 * Supports callbacks for open, message, and close events.
 */
class WebSocket : public std::enable_shared_from_this<WebSocket> {
public:
    /**
     * @brief Constructs a WebSocket object.
     * @tparam Socket Type of the underlying socket (TCP or SSL)
     * @param socket The connected socket
     * @param handlers Optional pointer to event handlers
     * @param client_id Unique ID of the client
     */
    template<typename Socket>
    WebSocket(Socket &&socket, const WsHandlers *handlers, size_t client_id)
        : socket_(AnySocket(std::forward<Socket>(socket))),
          client_id_(client_id),
          write_strand_(
              asio::make_strand(
                  std::visit(
                      [](auto &sock) { return sock.get_executor(); },
                      socket_
                  )
              )
          ) {
        if (handlers) handlers_ = *handlers;
    }

    /**
     * @brief Sets a cleanup function to be called when the WebSocket closes.
     * @param fn Function taking the client ID
     */
    void set_server_cleanup(std::function<void(size_t)> fn) {
        server_cleanup_ = std::move(fn);
    }

    /**
     * @brief Starts the WebSocket read loop and triggers the on_open handler.
     */
    asio::awaitable<void> start() {
        if (handlers_.on_open) handlers_.on_open(shared_from_this());
        co_await do_read_loop();
        co_return;
    }

    /**
     * @brief Closes the WebSocket immediately with the given code and reason.
     * @param code WebSocket close code
     * @param reason Close reason
     */
    void close(uint16_t code, const std::string_view &reason);

    /**
     * @brief Asynchronously closes the WebSocket by first sending a close frame.
     * @param code WebSocket close code
     * @param reason Close reason
     */
    void close_async(uint16_t code = 1000, std::string reason = {});

    /**
     * @brief Asynchronously sends a text message to the client.
     * @param payload Message content
     */
    void send_text_async(std::string payload);

    /**
     * @brief Asynchronously sends a binary message to the client.
     * @param payload Binary data
     */
    void send_binary_async(std::vector<uint8_t> payload);

    /**
     * @brief Returns the executor associated with this WebSocket.
     */
    asio::any_io_executor get_executor() const {
        return write_strand_;
    }

    /**
     * @brief Returns true if the WebSocket is open.
     */
    bool is_open() const noexcept {
        return state_.load(std::memory_order_acquire) == WS_OPEN;
    }

private:
    AnySocket socket_;                       ///< Underlying socket (TCP or SSL)
    size_t client_id_;                        ///< Unique client ID
    WsHandlers handlers_{};                  ///< Event handlers
    asio::cancellation_signal signal_;       ///< For cancelling async operations
    asio::strand<asio::any_io_executor> write_strand_; ///< Strand for serialized writes
    std::function<void(size_t)> server_cleanup_;      ///< Cleanup function called on close
    std::atomic<uint8_t> state_{WS_OPEN};    ///< Connection state
    std::deque<std::vector<uint8_t>> write_queue_; ///< Pending outgoing frames
    bool is_writing_ = false;                ///< Indicates if a write is in progress

    // -------------------- Internal helper functions ----------------------

    template<typename SocketType, typename Buffer>
    asio::awaitable<size_t> async_write_any(SocketType &st, const Buffer& buf);

    template<typename VariantSocket, typename MutableBuffer>
    asio::awaitable<size_t> async_read_any(VariantSocket &st, const MutableBuffer &buf);

    asio::awaitable<void> send_text(const std::string &payload);
    asio::awaitable<void> send_binary(const std::vector<uint8_t> &payload);
    asio::awaitable<void> send_pong(const std::string &payload);
    asio::awaitable<void> do_write(std::vector<uint8_t> bytes);
    asio::awaitable<void> do_read_loop();
    asio::awaitable<void> handle_parsed_frame(const WsFrame &f, FrameBody &fb);
    asio::awaitable<void> handle_close_frame(const std::string &payload);
    asio::awaitable<void> handle_ping_frame(const std::string &payload);
    asio::awaitable<void> send_close_with_reason(uint16_t code, std::string_view reason_utf8);
};

#endif
