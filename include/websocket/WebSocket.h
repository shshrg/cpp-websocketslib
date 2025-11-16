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

// WebSocket abstraction, used to represent one connection

enum: uint8_t {
    WS_OPEN = 0x0,
    WS_CLOSED = 0x1
};


using TcpSocket = asio::basic_stream_socket<asio::ip::tcp>;
#ifdef USE_SSL
#include <asio/ssl.hpp>
    using SslSocket = asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp> >;
    using AnySocket = std::variant<TcpSocket, SslSocket>;
#else
    using AnySocket = std::variant<TcpSocket>;
#endif


class WebSocket : std::enable_shared_from_this<WebSocket> {
public:
    template<typename Socket>
    WebSocket(Socket &&socket, const WsHandlers *handlers, asio::cancellation_slot token, size_t client_id)
        : socket_(AnySocket(std::forward<Socket>(socket))),
          slot_(token),
          client_id_(client_id) {
        if (handlers) handlers_ = *handlers;
        if (slot_.is_connected()) {
            slot_.assign(
                [this](asio::cancellation_type) {
                    std::error_code ec;
                    std::visit(
                        [&](auto &sock) {
                            using tcp = asio::ip::tcp;
                            auto &ll = sock.lowest_layer();
                            ll.shutdown(tcp::socket::shutdown_both, ec);
                            ll.close(ec);
                        },
                        socket_);
                }
            );
        }
    }

    // pass lambda which will be executed when closing
    void set_server_cleanup(std::function<void(size_t)> fn) {
        server_cleanup_ = std::move(fn);
    }


    asio::awaitable<void> start(const std::string &sec_ws_key) {
        std::string accept = ws_accept_key(sec_ws_key);
        Response resp = build_101_response(accept);

        std::string out = resp.to_string();
        co_await async_write_any(socket_, asio::buffer(resp.to_string()), slot_);

        if (handlers_.on_open) handlers_.on_open();

        co_await do_read_loop();
        // state_.store(WS_OPEN, std::memory_order_release);
        co_return;
    }

    // todo
    asio::awaitable<void> send(std::vector<uint8_t> bytes);

    void close(uint16_t code, const std::string_view &reason) {
        uint8_t expected = WS_OPEN;
        if (!state_.compare_exchange_strong(expected, WS_CLOSED,
                                            std::memory_order_acq_rel)) {
            return; // already closed
        }

        std::error_code ec;
        std::visit([&](auto &sock) {
            using tcp = asio::ip::tcp;
            sock.lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
            sock.lowest_layer().close(ec);
        }, socket_);

        if (handlers_.on_close)
            handlers_.on_close(code, reason);

        if (server_cleanup_)
            server_cleanup_(client_id_);
    }

private:
    AnySocket socket_;
    size_t client_id_;

    WsHandlers handlers_{};
    asio::cancellation_slot slot_;
    asio::cancellation_signal signal_;

    std::function<void(size_t)> server_cleanup_;

    std::atomic<uint8_t> state_{WS_OPEN};


    // ---------helper functions to handle different socket types----------
    template<typename SocketType, typename Buffer>
    asio::awaitable<std::size_t> async_write_any(SocketType &st, Buffer buf, asio::cancellation_slot slot) {
        co_return co_await std::visit(
            [&](auto &sock) -> asio::awaitable<std::size_t> {
                co_return co_await asio::async_write(
                    sock,
                    buf,
                    asio::bind_cancellation_slot(slot, asio::use_awaitable)
                );
            },
            st
        );
    }

    template<typename SocketType, typename Buffer>
    asio::awaitable<std::size_t> async_read_any(SocketType &st, Buffer buf, asio::cancellation_slot slot) {
        co_return co_await std::visit(
            [&](auto &sock) -> asio::awaitable<std::size_t> {
                co_return co_await sock.async_read_some(
                    buf,
                    asio::bind_cancellation_slot(slot, asio::use_awaitable)
                );
            },
            st
        );
    }

    // --------------------functions from ws_helpers-----------------------
    asio::awaitable<void> send_text(const std::string &payload) {
        WsFrame out{};
        out.fin = true;
        out.opcode = WS_TEXT;
        out.mask = false;
        out.payload_data.assign(payload.data(), payload.size());
        out.payload_length = out.payload_data.size();
        auto bytes = write_frame(out);
        co_await do_write(bytes);
        co_return;
    }

    asio::awaitable<void> send_pong(const std::string &payload) {
        WsFrame pong{};
        pong.fin = true;
        pong.opcode = WS_PONG;
        pong.mask = false;
        pong.payload_data.assign(payload.data(), payload.size());
        pong.payload_length = pong.payload_data.size();
        auto bytes = write_frame(pong);
        co_await do_write(bytes);
        co_return;
    }

    asio::awaitable<void> do_write(std::vector<uint8_t> const &bytes) {
        co_await async_write_any(socket_, asio::buffer(bytes), slot_);
        co_return;
    }

    asio::awaitable<void> do_read_loop() {
        constexpr size_t readChunk = 8 * 1024;
        size_t read_pos = 0;

        std::vector<uint8_t> inbuf;
        inbuf.reserve(readChunk);

        FrameBody fb{};

        auto compact_if = [&] {
            if (read_pos > inbuf.size() / 2 || read_pos > 64 * 1024) {
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
            WsFrame f{};
            size_t header_bytes = 0;

            for (;;) {
                const size_t available = inbuf.size() - read_pos;

                if (available >= 2) {
                    auto res = ws_try_parse_header(inbuf, read_pos, f, header_bytes);
                    if (res == HeaderParseResult::Ok) {
                        break;
                    }
                }
                ensure_free(readChunk);

                const size_t old = inbuf.size();
                inbuf.resize(old + readChunk);

                std::size_t n = co_await async_read_any(socket_,
                                                        asio::buffer(inbuf.data() + old, readChunk),
                                                        slot_);
                inbuf.resize(old + n);

                if (n == 0) {
                    close(1000, "eof");
                    co_return;
                }
            }
            const size_t frame_total = header_bytes + f.payload_length;

            for (;;) {
                const size_t available = inbuf.size() - read_pos;
                if (available >= frame_total) {
                    break;
                }

                ensure_free(readChunk);

                const size_t old = inbuf.size();
                inbuf.resize(old + readChunk);

                std::size_t n = co_await async_read_any(socket_,
                                                        asio::buffer(inbuf.data() + old, readChunk),
                                                        slot_);

                inbuf.resize(old + n);

                if (n == 0)
                    co_return;
            }
            const uint8_t *frame_ptr = inbuf.data() + read_pos;
            const uint8_t *payload_ptr = frame_ptr + header_bytes;
            const size_t payload_len = f.payload_length;

            if (payload_len > 0) {
                f.payload_data.resize(payload_len);
                if (f.mask) {
                    uint32_t key = f.masking_key;
                    for (size_t i = 0; i < payload_len; ++i) {
                        auto m = static_cast<uint8_t>(
                            (key >> ((3 - (i & 3)) * 8)) & 0xFF
                        );
                        f.payload_data[i] = static_cast<char>(payload_ptr[i] ^ m);
                    }
                } else {
                    std::memcpy(f.payload_data.data(),
                                payload_ptr,
                                payload_len);
                }
            } else {
                f.payload_data.clear();
            }
            co_await handle_parsed_frame(f, fb);

            read_pos += frame_total;
            compact_if();
        }
    }

    asio::awaitable<void> handle_parsed_frame(WsFrame &f, FrameBody &fb) {
        const uint8_t opcode = f.opcode;

        if (opcode == WS_PING) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(1002, "Fragmented control");
                co_return;
            }
            co_await handle_ping_frame(f.payload_data);
            co_return;
        }
        if (opcode == WS_PING) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(1002, "Fragmented control");
                co_return;
            }
            co_return;
        }
        if (opcode == WS_CLOSE) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(1002, "Fragmented control");
                co_return;
            }
            co_await handle_close_frame(f.payload_data);
            co_return;
        }
        if (opcode == WS_TEXT || opcode == WS_BINARY) {
            if (fb.assembling) {
                co_await send_close_with_reason(1002, "New data while assembling");
                if (handlers_.on_close) handlers_.on_close(1002, "New data while assembling");
                co_return;
            }
            fb.start(opcode, f.payload_length);
            fb.append(f.payload_data);

            if (f.fin) {
                if (fb.opcode == WS_TEXT) {
                    co_await handle_text_bytes(fb.msg);
                }
                fb.reset();
            }
            co_return;
        }
        if (opcode == WS_CONT) {
            if (!fb.assembling) {
                co_await send_close_with_reason(1002, "Continuattion without initial frame");
                if (handlers_.on_close) handlers_.on_close(1002, "Continuation without inital frame");
            }

            fb.append(f.payload_data);

            if (f.fin) {
                if (fb.opcode == WS_TEXT) {
                    co_await handle_text_bytes(fb.msg);
                }
                fb.reset();
            }
            co_return;
        }
    }

    asio::awaitable<void> handle_close_frame(const std::string &payload) {
        uint16_t code = 1000;
        std::string_view reason;
        if (payload.size() >= 2) {
            code = (static_cast<uint8_t>(payload[0]) << 8)
                   | static_cast<uint8_t>(payload[1]);
            if (payload.size() >= 2) {
                reason = std::string_view(payload.data() + 2, payload.size() - 2);
            }
        }
        co_await send_close_with_reason(code, reason);
        close(code, reason);
        co_return;
    }

    asio::awaitable<void> handle_ping_frame(const std::string &payload) {
        co_await send_pong(payload);
    }

    asio::awaitable<void> handle_text_bytes(const std::string &payload) {
        if (handlers_.on_message) handlers_.on_message(std::string(payload));
        co_await send_text(payload);
        co_return;
    }

    asio::awaitable<void> handle_text_frame(WsFrame &f);

    asio::awaitable<void> send_unsupported_and_close() {
        co_await send_close_with_reason(1003, "");
        co_return;
    }

    asio::awaitable<void> send_protocol_error_and_close() {
        co_await send_close_with_reason(1002, "");
        co_return;
    }

    asio::awaitable<void> send_close_code(uint16_t code, std::string_view reason_utf8) {
        co_await send_close_with_reason(code, reason_utf8);
    }

    asio::awaitable<void> send_close_with_reason(uint16_t code, std::string_view reason_utf8) {
        WsFrame out{};
        out.fin = true;
        out.opcode = WS_CLOSE;
        out.mask = false;
        out.payload_data = build_close_payload(code, reason_utf8);
        out.payload_length = out.payload_data.size();
        auto bytes = write_frame(out);
        co_await do_write(bytes);
        co_return;
    }
};


#endif
