#ifndef WEBSOCKETLIB_SESSION_H
#define WEBSOCKETLIB_SESSION_H

#include <asio.hpp>
#include <memory>
#include <iostream>
#include "io_helpers.h"
#include "http/request.h"
#include "http/response.h"
#include "http/utils.h"

struct Session : std::enable_shared_from_this<Session> {
    using tcp = asio::ip::tcp;

    std::shared_ptr<tcp::socket> socket;
    asio::cancellation_signal canceler;
    asio::cancellation_slot slot;

    explicit Session(std::shared_ptr<tcp::socket> s)
        : socket(std::move(s)), slot(canceler.slot()) {
    }

    asio::awaitable<void> run_once() {
        asio::streambuf buf;

        std::string headers_text = co_await co_read_headers(*socket, buf, slot);

        Request req;
        parse_http_request(headers_text, req);

        const std::size_t clen = req.content_length().value_or(0);
        if (clen) {
            req.body = co_await co_read_body(*socket, buf, clen, slot);
        }

        std::cout << req.to_string() << "\n";

        Response resp = Response::text("OK\n");
        std::string payload = resp.to_string();

        co_await asio::async_write(*socket, asio::buffer(payload),
                                   asio::bind_cancellation_slot(slot, asio::use_awaitable));
    }

    void cancel() { canceler.emit(asio::cancellation_type::all); }
};

#endif //WEBSOCKETLIB_SESSION_H
