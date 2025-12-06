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

class WebSocket;

using WsOpenHandler = std::function<void(std::shared_ptr<WebSocket>)>;
using WsMessageHandler = std::function<void(std::shared_ptr<WebSocket>, std::string_view msg)>;
using WsCloseHandler = std::function<void(std::shared_ptr<WebSocket>, uint16_t code, std::string_view reason)>;

struct WsHandlers {
    WsOpenHandler on_open{};
    WsMessageHandler on_message{};
    WsCloseHandler on_close{};
};

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


class WebSocket : public std::enable_shared_from_this<WebSocket> {
public:
    template<typename Socket>
    WebSocket(Socket &&socket, const WsHandlers *handlers, asio::cancellation_slot token, size_t client_id)
        : socket_(AnySocket(std::forward<Socket>(socket))),
          client_id_(client_id),
          slot_(token) {
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


        if (handlers_.on_open) handlers_.on_open(shared_from_this());


        co_await do_read_loop();
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
            handlers_.on_close(shared_from_this(), code, reason);

        if (server_cleanup_)
            server_cleanup_(client_id_);
    }

    void close_async(uint16_t code = 1000, std::string reason = {}) {
        auto self = shared_from_this();
        auto ex = std::visit(
            [](auto &sock) { return sock.get_executor(); },
            socket_
        );

        asio::co_spawn(
            ex,
            [self, code, reason = std::move(reason)]() mutable -> asio::awaitable<void> {
                co_await self->send_close_with_reason(code, reason);
                self->close(code, reason);
                co_return;
            },
            asio::detached
        );
    }

    void send_text_async(std::string payload) {
        auto self = shared_from_this();

        auto ex = std::visit(
            [](auto &sock) { return sock.get_executor(); },
            socket_
        );

        asio::co_spawn(
            ex,
            [self, payload = std::move(payload)]() mutable -> asio::awaitable<void> {
                co_await self->send_text(payload);
                co_return;
            },
            asio::detached
        );
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

    template<typename VariantSocket, typename MutableBuffer>
    asio::awaitable<std::size_t> async_read_any(
        VariantSocket &st,
        const MutableBuffer &buf,
        asio::cancellation_slot slot
    ) {
        co_return co_await std::visit(
            [&](auto &sock) -> asio::awaitable<std::size_t> {
                co_return co_await asio::async_read(
                    sock,
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
        constexpr size_t maxMsgSize = 1024 * 1024;

        std::array<uint8_t, 14> header{};

        FrameBody fb{maxMsgSize};

        while (true) {
            WsFrame f{};
            co_await async_read_any(socket_,
                                    asio::buffer(header.data(), 2),
                                    slot_);
            const uint8_t b0 = header[0];
            const uint8_t b1 = header[1];

            f.fin = (b0 & 0x80) != 0;
            f.opcode = (b0 & 0x0F);
            const bool mask = (b1 & 0x80) != 0;
            f.mask = mask;

            uint64_t len7 = (b1 & 0x7F);
            std::size_t header_bytes = 2;

            std::size_t ext_len_bytes = 0;
            if (len7 == 126) ext_len_bytes = 2;
            else if (len7 == 127) ext_len_bytes = 8;

            const std::size_t mask_bytes = mask ? 4 : 0;

            const std::size_t total_header_bytes =
                    header_bytes + ext_len_bytes + mask_bytes;

            // Additional info, like mask or extended length
            if (total_header_bytes > header_bytes) {
                co_await async_read_any(
                    socket_,
                    asio::buffer(header.data() + header_bytes,
                                 total_header_bytes - header_bytes),
                    slot_
                );
            }


            // Read extended length after the first two bytes
            const uint8_t *p = header.data() + 2;
            uint64_t payload_len = 0;

            if (ext_len_bytes == 0) {
                payload_len = len7;
            } else if (ext_len_bytes == 2) {
                payload_len = (static_cast<uint64_t>(p[0]) << 8) |
                              (static_cast<uint64_t>(p[1]));
                p += 2;
            } else {
                payload_len = 0;
                for (int i = 0; i < 8; ++i) {
                    payload_len = (payload_len << 8) | p[i];
                }
                p += 8;
            }

            f.payload_length = payload_len;

            uint32_t mask_key = 0;
            if (mask) {
                mask_key = (static_cast<uint32_t>(p[0]) << 24) |
                           (static_cast<uint32_t>(p[1]) << 16) |
                           (static_cast<uint32_t>(p[2]) << 8) |
                           static_cast<uint32_t>(p[3]);
                f.masking_key = mask_key;
            }

            f.payload_data.clear();
            f.payload_data.resize(payload_len);

            if (payload_len > 0) {
                co_await async_read_any(
                    socket_,
                    asio::buffer(f.payload_data.data(),
                                 f.payload_data.size()),
                    slot_
                );
            }

            if (mask && payload_len > 0) {
                uint32_t key = mask_key;
                auto *buf = reinterpret_cast<uint8_t *>(f.payload_data.data());

                for (std::size_t i = 0; i < f.payload_data.size(); ++i) {
                    auto m = static_cast<uint8_t>(
                        (key >> ((3 - (i & 3)) * 8)) & 0xFF
                    );
                    buf[i] = static_cast<uint8_t>(buf[i] ^ m);
                }
            }

            co_await handle_parsed_frame(f, fb);
        }
    }

    asio::awaitable<void> handle_parsed_frame(const WsFrame &f, FrameBody &fb) {
        const uint8_t opcode = f.opcode;

        auto too_big = [this](FrameBody &fb) -> asio::awaitable<void> {
            co_await send_close_with_reason(1009, "Message too big");
            if (handlers_.on_close)
                handlers_.on_close(shared_from_this(), 1009, "Message too big");
            fb.reset();
            co_return;
        };

        if (opcode == WS_PING) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(shared_from_this(), 1002, "Fragmented control");
                co_return;
            }
            co_await handle_ping_frame(f.payload_data);
            co_return;
        }
        if (opcode == WS_PING) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(shared_from_this(), 1002, "Fragmented control");
                co_return;
            }
            co_return;
        }
        if (opcode == WS_CLOSE) {
            if (!f.fin) {
                co_await send_close_with_reason(1002, "Fragmented control");
                if (handlers_.on_close) handlers_.on_close(shared_from_this(), 1002, "Fragmented control");
                co_return;
            }
            co_await handle_close_frame(f.payload_data);
            co_return;
        }
        if (opcode == WS_TEXT || opcode == WS_BINARY) {
            if (fb.assembling) {
                co_await send_close_with_reason(1002, "New data while assembling");
                if (handlers_.on_close) handlers_.on_close(shared_from_this(), 1002, "New data while assembling");
                co_return;
            }
            fb.start(opcode, f.payload_length);

            if (!fb.append(f.payload_data)) {
                co_await too_big(fb);
                co_return;
            }

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
                co_await send_close_with_reason(1002, "Continuation without initial frame");
                if (handlers_.on_close)
                    handlers_.on_close(shared_from_this(), 1002,
                                       "Continuation without initial frame");
            }

            if (!fb.append(f.payload_data)) {
                co_await too_big(fb);
                co_return;
            }

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
        if (handlers_.on_message) handlers_.on_message(shared_from_this(), std::string(payload));
        // co_await send_text(payload);
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
