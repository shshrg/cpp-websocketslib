#ifndef WEBSOCKETLIB_IO_HELPERS_H
#define WEBSOCKETLIB_IO_HELPERS_H

#include <asio.hpp>
#include <string>
#include <algorithm>


template <typename Socket>
int get_native_handle(Socket& socket) {
    if constexpr (std::is_same_v<Socket, asio::ip::tcp::socket>) {
        return socket.native_handle();
    }
    return -1; // Not supported for SSL
}

inline std::string prepare_headers(Response& response, std::uintmax_t body_size) {
    if (!response.has_header("Content-Length")) {
        response.set_header("Content-Length", std::to_string(body_size));
    }
    return response.to_string_header();
}

inline std::string take_front(asio::streambuf &buf, std::size_t n) {
    const auto avail = buf.size();
    n = std::min(n, avail);
    std::string out(n, '\0');
    auto bytes_copied = asio::buffer_copy(
        asio::buffer(out.data(), n),
        buf.data(),
        n
        );
    buf.consume(bytes_copied);
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
    std::string body(content_len, '\0');
    size_t already = 0;

    {
        const std::size_t avail = buffer.size();
        const std::size_t take = std::min(avail, content_len);
        if (take > 0) {
            auto seq = buffer.data();
            std::copy_n(
                asio::buffers_begin(seq),
                take,
                body.begin()
            );
            buffer.consume(take);
            already = take;
        }
    }
    size_t remaining = content_len - already;
    size_t done = 0;

    while (done < remaining) {
        std::size_t n = co_await asio::async_read(
            socket, asio::buffer(body.data() + already + done, remaining - done),
            asio::bind_cancellation_slot(token, asio::use_awaitable));

        if (n == 0) break;

        done += n;
    }

    if (already + done < content_len) {
        body.resize(already + done);
    }

    co_return body;
}

#endif //WEBSOCKETLIB_IO_HELPERS_H