#ifndef WEBSOCKETLIB_IO_HELPERS_H
#define WEBSOCKETLIB_IO_HELPERS_H

#include <asio.hpp>
#include <string>
#include <algorithm>

/**
 * @file io_helpers.h
 * @brief Utility functions for asynchronous I/O with ASIO.
 *
 * This header provides helpers for reading HTTP headers and body,
 * preparing response headers, and retrieving native socket handles.
 */

/**
 * @brief Get the native socket handle.
 *
 * Works only for plain TCP sockets. Returns -1 for SSL sockets.
 *
 * @tparam Socket Type of the socket.
 * @param socket Reference to the socket.
 * @return Native handle (file descriptor) if supported, -1 otherwise.
 */
template <typename Socket>
int get_native_handle(Socket& socket) {
    if constexpr (std::is_same_v<Socket, asio::ip::tcp::socket>) {
        return socket.native_handle();
    }
    return -1; // Not supported for SSL
}

/**
 * @brief Prepare the HTTP response headers for sending.
 *
 * Ensures the "Content-Length" header is set, and serializes the headers
 * to a string.
 *
 * @param response Response object to prepare.
 * @param body_size Size of the response body in bytes.
 * @return Serialized headers as a string (with trailing CRLF).
 */
inline std::string prepare_headers(Response& response, std::uintmax_t body_size) {
    if (!response.has_header("Content-Length")) {
        response.set_header("Content-Length", std::to_string(body_size));
    }
    return response.to_string_header();
}

/**
 * @brief Extract a given number of bytes from an ASIO stream buffer.
 *
 * Consumes the bytes from the buffer and returns them as a std::string.
 *
 * @param buf ASIO stream buffer.
 * @param n Number of bytes to extract.
 * @return Extracted bytes as a string.
 */
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

/**
 * @brief Asynchronously read HTTP headers from a socket until "\r\n\r\n".
 *
 * @tparam Socket Type of the socket.
 * @param socket ASIO socket to read from.
 * @param buffer Stream buffer to store incoming data.
 * @param token ASIO cancellation slot to allow aborting the read.
 * @return Awaitable yielding the header string including "\r\n\r\n".
 */
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

/**
 * @brief Asynchronously read the HTTP request/response body of given length.
 *
 * First consumes any bytes already present in the stream buffer, then
 * reads the remaining bytes from the socket.
 *
 * @tparam Socket Type of the socket.
 * @param socket ASIO socket to read from.
 * @param buffer Stream buffer that may already contain part of the body.
 * @param content_len Total length of the body to read.
 * @param token ASIO cancellation slot to allow aborting the read.
 * @return Awaitable yielding the complete body as a string.
 */
template <typename Socket>
asio::awaitable<std::string>
co_read_body(Socket &socket,
             asio::streambuf &buffer,
             std::size_t content_len,
             asio::cancellation_slot token) {
    std::string body(content_len, '\0');
    size_t already = 0;

    // Consume bytes already in buffer
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

    // Read remaining bytes from socket
    size_t remaining = content_len - already;
    size_t done = 0;

    while (done < remaining) {
        std::size_t n = co_await asio::async_read(
            socket, asio::buffer(body.data() + already + done, remaining - done),
            asio::bind_cancellation_slot(token, asio::use_awaitable));

        if (n == 0) break;

        done += n;
    }

    // Resize if fewer bytes were read
    if (already + done < content_len) {
        body.resize(already + done);
    }

    co_return body;
}

#endif // WEBSOCKETLIB_IO_HELPERS_H
