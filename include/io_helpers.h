#ifndef WEBSOCKETLIB_IO_HELPERS_H
#define WEBSOCKETLIB_IO_HELPERS_H

#include <asio.hpp>
#include <string>
#include <algorithm>

inline std::string take_front(asio::streambuf &buf, std::size_t n) {
    auto seq = buf.data();
    std::string out(n, '\0');
    std::copy_n(asio::buffers_begin(seq), n, out.begin());
    buf.consume(n);
    return out;
}


template <typename Socket>
 asio::awaitable<std::string>
co_read_headers(Socket &socket,
                asio::streambuf &buffer,
                asio::cancellation_slot token) {
    size_t header_bytes =
            co_await asio::async_read_until(socket, buffer, "\r\n\r\n",
                                            asio::bind_cancellation_slot(token, asio::use_awaitable));

    co_return take_front(buffer, header_bytes);
}

template <typename Socket>
 asio::awaitable<std::string>
co_read_body(Socket &socket,
             asio::streambuf &buffer,
             std::size_t content_len,
             asio::cancellation_slot token) {
    std::string body;
    body.reserve(content_len);

    {
        const std::size_t avail = buffer.size();
        const std::size_t take = std::min(avail, content_len);
        if (take) {
            body += take_front(buffer, take);
        }
    }

    while (body.size() < content_len) {
        const std::size_t need = content_len - body.size();

        std::size_t n = co_await asio::async_read(
            socket, buffer,
            asio::transfer_exactly(need),
            asio::bind_cancellation_slot(token, asio::use_awaitable));

        body += take_front(buffer, n);
    }

    co_return body;
}

#endif //WEBSOCKETLIB_IO_HELPERS_H