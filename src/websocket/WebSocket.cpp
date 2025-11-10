#include "websocket/WebSocket.h"

asio::awaitable<void> WebSocket::start(const std::string &sec_ws_key)
{
    std::string accept = ws_accept_key(sec_ws_key);
    Response resp = build_101_response(accept);
    
    std::string out = resp.to_string();

    co_await asio::async_write(socket_, asio::buffer(resp.to_string()),
                               asio::bind_cancellation_slot(slot_, asio::use_awaitable));

    // wrapper too
    if (handlers_.on_open) handlers_.on_open();

    co_await do_read_loop();

    state_.store(WS_OPEN, std::memory_order_release);
    co_return;
}

asio::awaitable<void> WebSocket::send(std::vector<uint8_t> bytes)
{
    // if 

    WsFrame frame;
    frame.fin = true;
    frame.opcode = WS_TEXT;
    frame.mask = false;
    frame.payload_data.assign(bytes.begin(), bytes.end());
    frame.payload_length = frame.payload_data.size();

    auto out_bytes = write_frame(frame);
    co_await asio::async_write(socket_, asio::buffer(out_bytes),
                               asio::bind_cancellation_slot(slot_, asio::use_awaitable));
}


asio::awaitable<void> WebSocket::do_read_loop()
{
    constexpr size_t read_chunk = 8 * 1024;
    size_t read_pos = 0;

    std::vector<uint8_t> inbuf;
    inbuf.reserve(read_chunk);

    auto compact_if = [&] {
        if (read_pos && (read_pos > inbuf.size() / 2 || read_pos > 64 * 1024))
        {
            const size_t remaining = inbuf.size() - read_pos;
            if (remaining)
                std::memmove(inbuf.data(), inbuf.data() + read_pos, remaining);
            inbuf.resize(remaining);
            read_pos = 0;
        }
    };
    auto ensure_free = [&](size_t min_free)
    {
        if (inbuf.capacity() - inbuf.size() < min_free)
            compact_if();
        if (inbuf.capacity() - inbuf.size() < min_free)
        {
            size_t want = inbuf.size() + std::max(min_free, inbuf.size());
            inbuf.reserve(want);
        }
    };

    while (true)
    {
        ensure_free(read_chunk);

        const size_t old = inbuf.size();
        inbuf.resize(old + read_chunk);

        std::size_t n = co_await socket_.async_read_some(
            asio::buffer(inbuf.data() + old, read_chunk),
            asio::bind_cancellation_slot(slot_, asio::use_awaitable)
        );

        inbuf.resize(old + n);

        while (true)
        {
            const size_t need = ws_next_frame_size(inbuf, read_pos);
            if (need == 0) break;

            const uint8_t* frame_ptr = inbuf.data() + read_pos;

            WsFrame frame;

            if (!parse_frame(frame_ptr, need, frame))
            {
                compact_if();
                co_await send_protocol_error_and_close();
                if (handlers_.on_close) handlers_.on_close(1002, "Protocol error");
                co_return;
            }
            read_pos += need;

            co_await handle_parsed_frame(std::move(frame));
        }
        compact_if();
    }
}

asio::awaitable<void> WebSocket::handle_parsed_frame(WsFrame &&f)
{
    // TODO handle fin = 0
    if (!f.fin || f.opcode == 0x0)
    {
        co_await send_unsupported_and_close();
        if (handlers_.on_close) handlers_.on_close(1003, "Unsupported (no fragmentation)");
        co_return;
    }

    switch (f.opcode)
    {
        case WS_TEXT:
            co_await handle_text_frame(f);
            break;
        case WS_PING:
            co_await handle_ping_frame(f);
            break;
        case WS_CLOSE:
            co_await handle_close_frame(f);
            co_return;
        default:
            co_await send_unsupported_and_close();
            if (handlers_.on_close)
                handlers_.on_close(1003, "Unsupported opcode");
            co_return;
    }
}

asio::awaitable<void> WebSocket::handle_close_frame(WsFrame &f)
{
    uint16_t code = 1000;
    std::string_view reason;
    if (f.payload_data.size() >= 2) {
        code = (static_cast<uint8_t>(f.payload_data[0]) << 8)
               | (static_cast<uint8_t>(f.payload_data[1]));
        reason = std::string_view(f.payload_data).substr(2);
    }
    auto bytes = write_frame(f);
    co_await do_write(bytes);
    if (handlers_.on_close) handlers_.on_close(code, reason);
    state_.store(WS_CLOSED, std::memory_order_release);
    co_return;
}

asio::awaitable<void> WebSocket::handle_ping_frame(WsFrame &f)
{
    WsFrame pong{};
    pong.fin = true;
    pong.opcode = WS_PONG;
    pong.mask = false;
    pong.payload_length = f.payload_data.size();
    pong.payload_data = f.payload_data;
    auto bytes = write_frame(pong);
    co_await do_write(bytes);
    co_return;
}

asio::awaitable<void> WebSocket::handle_text_frame(WsFrame &f)
{
    if (handlers_.on_message) handlers_.on_message(f.payload_data);

    WsFrame out{};
    out.fin = true;
    out.opcode = WS_TEXT;
    out.mask = false;
    out.payload_data = f.payload_data;
    out.payload_length = out.payload_data.size();

    auto bytes = write_frame(out);
    co_await do_write(bytes);
    co_return;
}

asio::awaitable<void> WebSocket::send_unsupported_and_close()
{
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_length = 2;
    out.payload_data = std::string("\x03\xEB", 2); // 1003
    auto bytes = write_frame(out);
    co_await do_write(bytes);
    if (handlers_.on_close) handlers_.on_close(1003, "Unsupported data");
    state_.store(WS_CLOSED, std::memory_order_release);
    co_return;
}

asio::awaitable<void> WebSocket::send_protocol_error_and_close()
{
    WsFrame out{};
    out.fin = true;
    out.opcode = WS_CLOSE;
    out.mask = false;
    out.payload_length = 2;
    out.payload_data = std::string("\x03\xEA", 2);
    auto bytes = write_frame(out);
    co_await do_write(bytes);
    if (handlers_.on_close) handlers_.on_close(1002, "Protocol error");
    state_.store(WS_CLOSED, std::memory_order_release);
    co_return;
}

asio::awaitable<void> WebSocket::close(uint16_t code, const std::string_view &reason)
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
    co_await do_write(bytes);

    state_.store(WS_CLOSED, std::memory_order_release);
    if (handlers_.on_close) handlers_.on_close(code, reason);
    // actually close connection
    socket_.close();

    
    co_return;
}

asio::awaitable<void> WebSocket::do_write(std::vector<uint8_t> const &bytes)
{
    co_await asio::async_write(socket_, asio::buffer(bytes), asio::bind_cancellation_slot(slot_, asio::use_awaitable));

    co_return;
}


template<typename Socket>
asio::awaitable<void> Server::process_session_ws(Socket &socket,
                                         const std::string &sec_ws_key,
                                         const WsHandlers *handlers,
                                         asio::cancellation_slot token,
                                         size_t client_id)
{
    auto ws = std::make_shared<WebSocket>(socket, shared_from_this(), handlers, token, client_id);
    register_websocket(client_id, ws);
    co_await ws.start(sec_ws_key);
    co_return;
    
}