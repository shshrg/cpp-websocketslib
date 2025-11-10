#ifndef WEB_SOCKET_H
#define WEB_SOCKET_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <atomic>
#include "ws_helpers.h"
#include "WsFrame.h"
#include "server.h"
#include "http/response.h"
#include "http/utils.h"

enum: uint8_t {
    WS_OPEN = 0x0,
    WS_CLOSED = 0x1
};

class Server;

using TcpSocket = asio::ip::tcp::socket;
using SslSocket = asio::ssl::stream<asio::ip::tcp::socket>;
using AnySocket = std::variant<TcpSocket, SslSocket>;


class WebSocket : std::enable_shared_from_this<WebSocket>
{
public:
    WebSocket(TcpSocket &socket, std::shared_ptr<Server> server, const WsHandlers *handlers, asio::cancellation_slot token, size_t client_id)
    : socket_(std::move(socket)),
      server_(std::move(server)),
      slot_(token),
      client_id_(client_id)
    {
        if (handlers) handlers_ = *handlers;
    }
    asio::awaitable<void> start(const std::string &sec_ws_key);
    asio::awaitable<void> send(std::vector<uint8_t> bytes);
    asio::awaitable<void> close(uint16_t code, const std::string_view &reason);
private:
    TcpSocket socket_;
    std::weak_ptr<Server> server_;
    size_t client_id_;

    WsHandlers handlers_{};
    asio::cancellation_slot slot_;
    asio::cancellation_signal signal_;

    // for partial reads/writes and frames with fin=0 TODO
    std::vector<uint8_t> read_buffer_;
    std::vector<uint8_t> write_buffer_;
    // size_t read_pos;

    std::atomic<uint8_t> state_{WS_CLOSED};
    
    // data about the websocket
    std::string path;
    std::string client_id;

    // functions from ws_helpers
    asio::awaitable<void> do_read_loop();
    asio::awaitable<void> do_write(std::vector<uint8_t> const &bytes);
    asio::awaitable<void> handle_parsed_frame(WsFrame &&f);
    asio::awaitable<void> handle_close_frame(WsFrame &f);
    asio::awaitable<void> handle_ping_frame(WsFrame &f);
    asio::awaitable<void> handle_text_frame(WsFrame &f);
    asio::awaitable<void> send_unsupported_and_close();
    asio::awaitable<void> send_protocol_error_and_close();
};



#endif