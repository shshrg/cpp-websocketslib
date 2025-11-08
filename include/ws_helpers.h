#ifndef WS_HELPERS
#define WS_HELPERS

#include <asio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include "websocket/WsFrame.h"
#include <algorithm>
#include <span>

using WsOpenHandler = std::function<void()>;
using WsMessageHandler = std::function<void(std::string_view msg)>;
using WsCloseHandler = std::function<void(uint16_t code, std::string_view reason)>;

struct WsHandlers {
    WsOpenHandler on_open{};
    WsMessageHandler on_message{};
    WsCloseHandler on_close{};
};


struct FrameBody {
    bool assembling = false;
    uint8_t opcode = 0;
    std::string msg;
    size_t limit = 16 * 1024 * 1024;

    void reset() {
        assembling = false;
        opcode = 0;
        msg.clear();
    }

    void start(uint8_t op, size_t reserve_hint = 0) {
        assembling = true;
        opcode = op;
        msg.clear();
        if (reserve_hint) msg.reserve(std::min(limit, reserve_hint));
    }

    [[nodiscard]] bool would_exceed(size_t add) const {
        return msg.size() + add > limit;
    }

    [[nodiscard]] bool append(std::span<const uint8_t> bytes) {
        if (would_exceed(bytes.size())) return false;
        msg.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        return true;
    }

    [[nodiscard]] bool append(const uint8_t *data, size_t n) {
        if (would_exceed(n)) return false;
        msg.append(reinterpret_cast<const char *>(data), n);
        return true;
    }

    [[nodiscard]] bool append(std::string_view sv) {
        if (would_exceed(sv.size())) return false;
        msg.append(sv.data(), sv.size());
        return true;
    }

    [[nodiscard]] std::string_view as_string_view() const {
        return std::string_view{msg.data(), msg.size()};
    }

    [[nodiscard]] std::span<const uint8_t> as_span() const {
        return {
            reinterpret_cast<const uint8_t *>(msg.data()), msg.size()
        };
    }

    [[nodiscard]] size_t size() const { return msg.size(); }
    [[nodiscard]] bool empty() const { return msg.empty(); }
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
                               const std::vector<uint8_t> &bytes,
                               asio::cancellation_slot token) {
    co_await asio::async_write(socket,
                               asio::buffer(bytes),
                               asio::bind_cancellation_slot(token, asio::use_awaitable));
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_pong(Socket &socket,
                                std::string_view payload,
                                asio::cancellation_slot token) {
    WsFrame pong{};
    pong.fin = true;
    pong.opcode = WS_PONG;
    pong.mask = false;
    pong.payload_data.assign(payload.data(), payload.size());
    pong.payload_length = pong.payload_data.size();
    auto bytes = write_frame(pong);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_text(Socket &socket,
                                std::string_view payload,
                                asio::cancellation_slot token) {
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_TEXT;
    out.mask = false;
    out.payload_data.assign(payload.data(), payload.size());
    out.payload_length = out.payload_data.size();
    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

inline std::string build_close_payload(uint16_t code, std::string_view reason_utf8) {
    std::string out;
    out.resize(2);
    out[0] = static_cast<char>((code >> 8) & 0xFF);
    out[1] = static_cast<char>(code & 0xFF);
    const size_t room = 125 - 2; // 123
    if (!reason_utf8.empty()) {
        const size_t take = std::min(room, reason_utf8.size());
        out.append(reason_utf8.data(), take);
    }
    return out;
}

template<typename Socket>
asio::awaitable<void> send_close_with_reason(Socket &socket,
                                             uint16_t code,
                                             std::string_view reason_utf8,
                                             asio::cancellation_slot token) {
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_data = build_close_payload(code, reason_utf8);
    out.payload_length = out.payload_data.size();
    auto bytes = write_frame(out);
    co_await do_write(socket, bytes, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_close_code(Socket &socket,
                                      uint16_t code,
                                      std::string_view reason_utf8,
                                      asio::cancellation_slot token) {
    co_await send_close_with_reason(socket, code, reason_utf8, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_protocol_error_and_close(Socket &socket,
                                                    asio::cancellation_slot token) {
    co_await send_close_with_reason(socket, 1002, "", token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> send_unsupported_and_close(Socket &socket,
                                                 asio::cancellation_slot token) {
    co_await send_close_with_reason(socket, 1003, "", token);
    co_return;
}


template<typename Socket>
asio::awaitable<void> send_close_code(Socket &socket,
                                      uint16_t code,
                                      const std::string_view &reason,
                                      asio::cancellation_slot token) {
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
asio::awaitable<void> handle_text_bytes(Socket &socket,
                                        std::string_view payload,
                                        const WsHandlers *handlers,
                                        asio::cancellation_slot token) {
    if (handlers && handlers->on_message) handlers->on_message(std::string(payload));
    co_await send_text(socket, payload, token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_ping_frame(Socket &socket,
                                        const WsFrame &f,
                                        asio::cancellation_slot token) {
    co_await send_pong(socket, std::string_view(f.payload_data.data(), f.payload_data.size()), token);
    co_return;
}

template<typename Socket>
asio::awaitable<void> handle_close_frame(Socket &socket,
                                         const WsFrame &f,
                                         const WsHandlers *handlers,
                                         asio::cancellation_slot token) {
    uint16_t code = 1000;
    std::string_view reason;
    if (f.payload_data.size() >= 2) {
        code = (static_cast<uint8_t>(f.payload_data[0]) << 8)
             |  static_cast<uint8_t>(f.payload_data[1]);
        reason = std::string_view(f.payload_data).substr(2);
    }
    co_await send_close_with_reason(socket, code, reason, token);
    if (handlers && handlers->on_close) handlers->on_close(code, reason);
    co_return;
}


template<typename Socket>
asio::awaitable<void> handle_text_frame(Socket &socket,
                                        const WsFrame &f,
                                        const WsHandlers *handlers,
                                        asio::cancellation_slot token) {
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
asio::awaitable<void> handle_parsed_frame(Socket &socket,
                                          WsFrame &&f,
                                          const WsHandlers *handlers,
                                          FrameBody &fb,
                                          asio::cancellation_slot token) {
    if (f.opcode == WS_PING) {
        if (!f.fin) {
            co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
            if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
            co_return;
        }
        co_await handle_ping_frame(socket, f, token);
        co_return;
    }
    if (f.opcode == WS_PONG) {
        if (!f.fin) {
            co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
            if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
            co_return;
        }
        co_return;
    }
    if (f.opcode == WS_CLOSE) {
        if (!f.fin) {
            co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
            if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
            co_return;
        }
        co_await handle_close_frame(socket, f, handlers, token);
        co_return;
    }

    const auto sv = std::string_view(f.payload_data.data(), f.payload_data.size());

    if (f.opcode == WS_TEXT || f.opcode == WS_BINARY) {
        if (fb.assembling) {
            co_await send_close_with_reason(socket, 1002, "New data while assembling", token);
            if (handlers && handlers->on_close) handlers->on_close(1002, "New data while assembling");
            co_return;
        }
        fb.start(f.opcode, f.payload_data.size());
        if (!fb.append(sv)) {
            co_await send_close_with_reason(socket, 1009, "Message too big", token);
            if (handlers && handlers->on_close) handlers->on_close(1009, "Message too big");
            co_return;
        }

        if (f.fin) {
            if (fb.opcode == WS_TEXT) {
                co_await handle_text_bytes(socket, fb.as_string_view(), handlers, token);
                fb.reset();
            }
            co_return;
        }
    }

    if (f.opcode == WS_CONT) {
        if (!fb.append(sv)) {
            co_await send_close_with_reason(socket, 1009, "Message too big", token);
            if (handlers && handlers->on_close) handlers->on_close(1009, "Message too big");
            co_return;
        }
        if (f.fin) {
            if (fb.opcode == WS_TEXT) {
                co_await handle_text_bytes(socket, fb.as_string_view(), handlers, token);
                fb.reset();
            }
            co_return;
        }

        co_await send_unsupported_and_close(socket, token);
        if (handlers && handlers->on_close) handlers->on_close(1003, "Unsupported opcode");
        co_return;
    }
}


template<typename Socket>
asio::awaitable<void> do_read_loop(Socket &socket,
                                   const WsHandlers *handlers,
                                   asio::cancellation_slot token) {
    constexpr size_t readChunk = 8 * 1024;
    size_t read_pos = 0;

    std::vector<uint8_t> inbuf;
    inbuf.reserve(readChunk);

    FrameBody fb{};

    auto compact_if = [&] {
        if (read_pos && (read_pos > inbuf.size() / 2 || read_pos > 64 * 1024)) {
            const size_t remaining = inbuf.size() - read_pos;
            if (remaining)
                std::memmove(inbuf.data(), inbuf.data() + read_pos, remaining);
            inbuf.resize(remaining);
            read_pos = 0;
        }
    };
    auto ensure_free = [&](size_t min_free) {
        if (inbuf.capacity() - inbuf.size() < min_free) {
            compact_if();
        }
        if (inbuf.capacity() - inbuf.size() < min_free) {
            size_t want = inbuf.size() + std::max(min_free, inbuf.size());
            inbuf.reserve(want);
        }
    };

    while (true) {
        ensure_free(readChunk);

        const size_t old = inbuf.size();
        inbuf.resize(old + readChunk);

        std::size_t n = co_await socket.async_read_some(
            asio::buffer(inbuf.data() + old, readChunk),
            asio::bind_cancellation_slot(token, asio::use_awaitable));

        inbuf.resize(old + n);

        while (true) {
            const size_t need = ws_next_frame_size(inbuf, read_pos);
            if (need == 0) break;

            const uint8_t *frame_ptr = inbuf.data() + read_pos;

            WsFrame frame;

            if (!parse_frame(frame_ptr, need, frame)) {
                compact_if();
                co_await send_protocol_error_and_close(socket, token);
                if (handlers && handlers->on_close) handlers->on_close(1002, "Protocol error");
                co_return;
            }
            read_pos += need;


            co_await handle_parsed_frame(socket, std::move(frame), handlers, fb, token);
        }

        compact_if();
    }
}


#endif //WS_HELPERS
