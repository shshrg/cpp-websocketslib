#ifndef WS_HELPERS
#define WS_HELPERS
#pragma once

#include <asio.hpp>
#include <vector>
#include <array>
#include <string>
#include "websocket/WsFrame.h"
#include <optional>

using WsOpenHandler = std::function<void()>;
using WsMessageHandler = std::function<void(std::string_view msg)>;
using WsCloseHandler = std::function<void(uint16_t code, std::string_view reason)>;

struct WsHandlers {
    WsOpenHandler on_open{};
    WsMessageHandler on_message{};
    WsCloseHandler on_close{};
};

enum : uint8_t {
    WS_CONT = 0x0,
    WS_TEXT = 0x1,
    WS_BINARY = 0x2,
    WS_CLOSE = 0x8,
    WS_PING = 0x9,
    WS_PONG = 0xA
};

template<typename Socket>
asio::awaitable<void> do_write(Socket &socket,
                               std::vector<uint8_t> const &bytes,
                               asio::cancellation_slot token)
{
    co_await asio::async_write(socket,
                               asio::buffer(bytes),
                               asio::bind_cancellation_slot(token, asio::use_awaitable));
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_close_code(Socket &socket,
                                      uint16_t code,
                                      const std::string_view &reason,
                                      asio::cancellation_slot token)
{
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_length = 2 + reason.size();
    out.payload_data.resize(2 + reason.size());
    out.payload_data[0] = static_cast<char>((code >> 8) & 0xFF);
    out.payload_data[1] = static_cast<char>(code & 0xFF);
    if (!reason.empty()) {
        std::copy(reason.begin(), reason.end(), out.payload_data.begin() + 2);
    }
    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_protocol_error_and_close(Socket &socket,
                                                    asio::cancellation_slot token)
{
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_length = 2;
    out.payload_data = std::string("\x03\xEA", 2);
    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_unsupported_and_close(Socket &socket,
                                                 asio::cancellation_slot token)
{
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_length = 2;
    out.payload_data = std::string("\x03\xEB", 2); // 1003
    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_text_frame(Socket &socket,
                                        const WsFrame &f,
                                        const WsHandlers *handlers,
                                        asio::cancellation_slot token)
{
    if (handlers && handlers->on_message) handlers->on_message(f.payload_data);

    WsFrame out{};
    out.fin = true;
    out.opcode = WS_TEXT;
    out.mask = false;
    out.payload_data = f.payload_data;
    out.payload_length = out.payload_data.size();

    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_ping_frame(Socket &socket,
                                        const WsFrame &f,
                                        asio::cancellation_slot token)
{
    WsFrame pong{};
    pong.fin = true;
    pong.opcode = WS_PONG;
    pong.mask = false;
    pong.payload_length = f.payload_data.size();
    pong.payload_data = f.payload_data;
    auto bytes = write_frame(pong);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_close_frame(Socket &socket,
                                         const WsFrame &f,
                                         const WsHandlers *handlers,
                                         asio::cancellation_slot token)
{
    uint16_t code = 1000;
    std::string_view reason;
    if (f.payload_data.size() >= 2) {
        code = (static_cast<uint8_t>(f.payload_data[0]) << 8)
               | (static_cast<uint8_t>(f.payload_data[1]));
        reason = std::string_view(f.payload_data).substr(2);
    }
    auto bytes = write_frame(f);
    co_await do_write(socket, bytes, token);
    if (handlers && handlers->on_close) handlers->on_close(code, reason);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_parsed_frame(Socket &socket,
                                          WsFrame &&f,
                                          const WsHandlers *handlers,
                                          asio::cancellation_slot token)
{
    if (!f.fin || f.opcode == 0x0) {
        co_await send_unsupported_and_close(socket, token);
        if (handlers && handlers->on_close) handlers->on_close(1003, "Unsupported (no fragmentation)");
        co_return;
    }

    switch (f.opcode) {
        case WS_TEXT:
            co_await handle_text_frame(socket, f, handlers, token);
            break;
        case WS_PING:
            co_await handle_ping_frame(socket, f, token);
            break;
        case WS_CLOSE:
            co_await handle_close_frame(socket, f, handlers, token);
            co_return;
        default:
            co_await send_unsupported_and_close(socket, token);
            if (handlers && handlers->on_close) handlers->on_close(1003, "Unsupported opcode");
            co_return;
    }

    co_return;
}

template<typename Socket>
asio::awaitable<void> do_read_loop(Socket &socket,
                                   const WsHandlers *handlers,
                                   asio::cancellation_slot token)
{
    std::vector<uint8_t> inbuf;
    inbuf.reserve(4096);
    std::array<uint8_t, 4096> tmp{};

    while (true) {
        std::size_t n = co_await socket.async_read_some(asio::buffer(tmp),
                                                        asio::bind_cancellation_slot(token, asio::use_awaitable));
        inbuf.insert(inbuf.end(), tmp.begin(), tmp.begin() + n);

        while (true) {
            size_t need = ws_next_frame_size(inbuf);
            if (need == 0) break;

            std::vector<uint8_t> frame_bytes;
            frame_bytes.reserve(need);
            frame_bytes.insert(frame_bytes.end(), inbuf.begin(), inbuf.begin() + static_cast<off_t>(need));
            inbuf.erase(inbuf.begin(), inbuf.begin() + static_cast<off_t>(need));

            auto maybe = parse_frame(std::move(frame_bytes));
            if (!maybe) {
                co_await send_protocol_error_and_close(socket, token);
                if (handlers && handlers->on_close) handlers->on_close(1002, "Protocol error");
                co_return;
            }
            co_await handle_parsed_frame(socket, std::move(*maybe), handlers, token);
        }
    }
}

#endif //WS_HELPERS
