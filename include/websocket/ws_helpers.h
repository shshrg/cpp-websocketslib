#ifndef WS_HELPERS
#define WS_HELPERS

#include <asio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include "websocket/WsFrame.h"
#include <algorithm>

using WsOpenHandler = std::function<void()>;
using WsMessageHandler = std::function<void(std::string_view msg)>;
using WsCloseHandler = std::function<void(uint16_t code, std::string_view reason)>;

// TODO: wrapper for on_close
struct WsHandlers {
    WsOpenHandler on_open{};
    WsMessageHandler on_message{};
    WsCloseHandler on_close{};
};

struct FrameBody {
    bool assembling = false;
    uint8_t opcode = 0;
    std::string msg;

    void reset() noexcept {
        assembling = false;
        opcode = 0;
        msg.clear();
    }

    void start(uint8_t op, size_t reserve_hint = 0) {
        assembling = true;
        opcode = op;
        msg.clear();
        if (reserve_hint)
            msg.reserve(reserve_hint);
    }

    bool append(const std::string &bytes) {
        msg.append(bytes.data(), bytes.size());
        return true;
    }

    bool append_masked(const uint8_t *data, size_t len, uint32_t mask_key) {
        const size_t old_size = msg.size();
        msg.resize(old_size + len);
        for (size_t i = 0; i < len; ++i) {
            auto m = static_cast<uint8_t>(
                (mask_key >> ((3 - (i & 3)) * 8)) & 0xFF
            );
            msg[old_size + i] = static_cast<char>(data[i] ^ m);
        }
        return true;
    }


    [[nodiscard]] size_t size() const noexcept { return msg.size(); }
    [[nodiscard]] bool empty() const noexcept { return msg.empty(); }
};



enum : uint8_t {
    WS_CONT = 0x0,
    WS_TEXT = 0x1,
    WS_BINARY = 0x2,
    WS_CLOSE = 0x8,
    WS_PING = 0x9,
    WS_PONG = 0xA
};
// template<typename Socket>
// asio::awaitable<void> do_write(Socket &socket,
//                                const std::vector<uint8_t> &bytes,
//                                asio::cancellation_slot token) {
//     co_await asio::async_write(socket,
//                                asio::buffer(bytes),
//                                asio::bind_cancellation_slot(token, asio::use_awaitable));
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> send_pong(Socket &socket,
//                                 const std::string & payload,
//                                 asio::cancellation_slot token) {
//     WsFrame pong{};
//     pong.fin = true;
//     pong.opcode = WS_PONG;
//     pong.mask = false;
//     pong.payload_data.assign(payload.data(), payload.size());
//     pong.payload_length = pong.payload_data.size();
//     auto bytes = write_frame(pong);
//     co_await do_write(socket, bytes, token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> send_text(Socket &socket,
//                                 const std::string & payload,
//                                 asio::cancellation_slot token) {
//     WsFrame out{};
//     out.fin = true;
//     out.opcode = WS_TEXT;
//     out.mask = false;
//     out.payload_data.assign(payload.data(), payload.size());
//     out.payload_length = out.payload_data.size();
//     auto bytes = write_frame(out);
//     co_await do_write(socket, bytes, token);
//     co_return;
// }

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

// template<typename Socket>
// asio::awaitable<void> send_close_with_reason(Socket &socket,
//                                              uint16_t code,
//                                              std::string_view reason_utf8,
//                                              asio::cancellation_slot token) {
//     WsFrame out{};
//     out.fin = true;
//     out.opcode = WS_CLOSE;
//     out.mask = false;
//     out.payload_data = build_close_payload(code, reason_utf8);
//     out.payload_length = out.payload_data.size();
//     auto bytes = write_frame(out);
//     co_await do_write(socket, bytes, token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> send_close_code(Socket &socket,
//                                       uint16_t code,
//                                       std::string_view reason_utf8,
//                                       asio::cancellation_slot token) {
//     co_await send_close_with_reason(socket, code, reason_utf8, token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> send_protocol_error_and_close(Socket &socket,
//                                                     asio::cancellation_slot token) {
//     co_await send_close_with_reason(socket, 1002, "", token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> send_unsupported_and_close(Socket &socket,
//                                                  asio::cancellation_slot token) {
//     co_await send_close_with_reason(socket, 1003, "", token);
//     co_return;
// }


// template<typename Socket>
// asio::awaitable<void> handle_text_bytes(Socket &socket,
//                                         const std::string & payload,
//                                         const WsHandlers *handlers,
//                                         asio::cancellation_slot token) {
//     if (handlers && handlers->on_message) handlers->on_message(std::string(payload));
//     co_await send_text(socket, payload, token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> handle_ping_frame(Socket &socket,
//                                         const std::string & payload,
//                                         asio::cancellation_slot token) {
//     co_await send_pong(socket, payload, token);
//     co_return;
// }

// template<typename Socket>
// asio::awaitable<void> handle_close_frame(Socket &socket,
//                                          const std::string & payload,
//                                          const WsHandlers *handlers,
//                                          asio::cancellation_slot token) {
//     uint16_t code = 1000;
//     std::string_view reason;
//     if (payload.size() >= 2) {
//         code = (static_cast<uint8_t>(payload[0]) << 8)
//              |  static_cast<uint8_t>(payload[1]);
//         if (payload.size() > 2) {
//             reason = std::string_view(payload.data() + 2, payload.size() - 2);
//         }
//     }
//     co_await send_close_with_reason(socket, code, reason, token);
//     if (handlers && handlers->on_close) handlers->on_close(code, reason);
//     co_return;
// }


// template<typename Socket>
// asio::awaitable<void> handle_parsed_frame(Socket &socket,
//                                           const WsFrame &f,
//                                           const WsHandlers *handlers,
//                                           FrameBody &fb,
//                                           asio::cancellation_slot token)
// {
//     const uint8_t opcode = f.opcode;

//     if (opcode == WS_PING) {
//         if (!f.fin) {
//             co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
//             if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
//             co_return;
//         }
//         co_await handle_ping_frame(socket, f.payload_data, token);
//         co_return;
//     }

//     if (opcode == WS_PONG) {
//         if (!f.fin) {
//             co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
//             if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
//             co_return;
//         }
//         co_return;
//     }

//     if (opcode == WS_CLOSE) {
//         if (!f.fin) {
//             co_await send_close_with_reason(socket, 1002, "Fragmented control", token);
//             if (handlers && handlers->on_close) handlers->on_close(1002, "Fragmented control");
//             co_return;
//         }
//         co_await handle_close_frame(socket, f.payload_data, handlers, token);
//         co_return;
//     }

//     if (opcode == WS_TEXT || opcode == WS_BINARY) {
//         if (fb.assembling) {
//             co_await send_close_with_reason(socket, 1002, "New data while assembling", token);
//             if (handlers && handlers->on_close) handlers->on_close(1002, "New data while assembling");
//             co_return;
//         }
//         fb.start(opcode, f.payload_length);
//         fb.append(f.payload_data);

//         if (f.fin) {
//             if (fb.opcode == WS_TEXT) {
//                 co_await handle_text_bytes(socket, fb.msg, handlers, token);
//             }
//             fb.reset();
//         }
//         co_return;
//     }

//     if (opcode == WS_CONT) {
//         if (!fb.assembling) {
//             co_await send_close_with_reason(socket, 1002, "Continuation without initial frame", token);
//             if (handlers && handlers->on_close) handlers->on_close(1002, "Continuation without initial frame");
//             co_return;
//         }

//         fb.append(f.payload_data);

//         if (f.fin) {
//             if (fb.opcode == WS_TEXT) {
//                 co_await handle_text_bytes(socket, fb.msg, handlers, token);
//             }
//             fb.reset();
//         }
//         co_return;
//     }

//     co_await send_unsupported_and_close(socket, token);
//     if (handlers && handlers->on_close) handlers->on_close(1003, "Unsupported opcode");
//     co_return;
// }


// template<typename Socket>
// asio::awaitable<void> do_read_loop(Socket &socket,
//                                    const WsHandlers *handlers,
//                                    asio::cancellation_slot token)
// {
//     constexpr size_t readChunk = 8 * 1024;
//     size_t read_pos = 0;

//     std::vector<uint8_t> inbuf;
//     inbuf.reserve(readChunk);

//     FrameBody fb{};

//     auto compact_if = [&] {
//         if (read_pos && (read_pos > inbuf.size() / 2 || read_pos > 64 * 1024)) {
//             const size_t remaining = inbuf.size() - read_pos;
//             if (remaining)
//                 std::memmove(inbuf.data(), inbuf.data() + read_pos, remaining);
//             inbuf.resize(remaining);
//             read_pos = 0;
//         }
//     };

//     auto ensure_free = [&](size_t min_free) {
//         if (inbuf.capacity() - inbuf.size() < min_free) {
//             compact_if();
//         }
//         if (inbuf.capacity() - inbuf.size() < min_free) {
//             size_t want = inbuf.size() + std::max(min_free, inbuf.size());
//             inbuf.reserve(want);
//         }
//     };

//     while (true) {
//         WsFrame f{};
//         size_t header_bytes = 0;

//         for (;;) {
//             const size_t available = inbuf.size() - read_pos;

//             if (available >= 2) {
//                 auto res = ws_try_parse_header(inbuf, read_pos, f, header_bytes);
//                 if (res == HeaderParseResult::Ok) {
//                     break;
//                 }
//             }

//             ensure_free(readChunk);

//             const size_t old = inbuf.size();
//             inbuf.resize(old + readChunk);

//             std::size_t n = co_await socket.async_read_some(
//                 asio::buffer(inbuf.data() + old, readChunk),
//                 asio::bind_cancellation_slot(token, asio::use_awaitable)
//             );

//             inbuf.resize(old + n);

//             if (n == 0) {
//                 co_return;
//             }
//         }

//         const size_t frame_total =
//             header_bytes + f.payload_length;

//         for (;;) {
//             const size_t available = inbuf.size() - read_pos;
//             if (available >= frame_total) {
//                 break;
//             }

//             ensure_free(readChunk);

//             const size_t old = inbuf.size();
//             inbuf.resize(old + readChunk);

//             std::size_t n = co_await socket.async_read_some(
//                 asio::buffer(inbuf.data() + old, readChunk),
//                 asio::bind_cancellation_slot(token, asio::use_awaitable)
//             );

//             inbuf.resize(old + n);

//             if (n == 0) {
//                 co_return;
//             }
//         }

//         const uint8_t* frame_ptr   = inbuf.data() + read_pos;
//         const uint8_t* payload_ptr = frame_ptr + header_bytes;
//         const size_t   payload_len =
//             f.payload_length;

//         if (payload_len > 0) {
//             f.payload_data.resize(payload_len);
//             if (f.mask) {
//                 uint32_t key = f.masking_key;
//                 for (size_t i = 0; i < payload_len; ++i) {
//                     auto m = static_cast<uint8_t>(
//                         (key >> ((3 - (i & 3)) * 8)) & 0xFF
//                     );
//                     f.payload_data[i] = static_cast<char>(payload_ptr[i] ^ m);
//                 }
//             } else {
//                 std::memcpy(f.payload_data.data(),
//                             payload_ptr,
//                             payload_len);
//             }
//         } else {
//             f.payload_data.clear();
//         }

//         co_await handle_parsed_frame(
//             socket,
//             f,
//             handlers,
//             fb,
//             token
//         );

//         read_pos += frame_total;
//         compact_if();
//     }
// }

#endif //WS_HELPERS
