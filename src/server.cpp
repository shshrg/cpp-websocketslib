#include "server.h"
#include <iostream>
#include <thread>
#include <string>
#include "http/request.h"
#include "http/response.h"
#include "http/utils.h"

Server::Server(asio::io_context &io_context, unsigned short port)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
}

void Server::start() {
    std::lock_guard lock(mutex_);
    if (is_running_) return;
    is_running_ = true;
    work_guard_.emplace(asio::make_work_guard(io_context_));

    const auto global_token_ = cancellation_signal_.slot();
    asio::co_spawn(io_context_, do_accept(global_token_), asio::detached);


    for (int i = 0; i < 7; ++i) {
        workers_.emplace_back([this] { io_context_.run(); });
    }
}

void Server::stop() {
    std::lock_guard lock(mutex_);
    if (!is_running_) return;
    cancellation_signal_.emit(asio::cancellation_type::all);

    if (work_guard_) {
        work_guard_.reset();
    }

    for (auto &t: workers_) {
        if (t.joinable()) t.join();
    }

    workers_.clear();


    is_running_ = false;
}

void Server::cancel_all() {
    cancellation_signal_.emit(asio::cancellation_type::all);
}

template<typename CancellationToken>
asio::awaitable<void> Server::do_accept(CancellationToken token) {
    auto socket = std::make_shared<asio::ip::tcp::socket>(co_await asio::this_coro::executor);
    co_await acceptor_.async_accept(
        *socket,
        asio::bind_cancellation_slot(token, asio::use_awaitable)
    );
    std::cout << "New client connected\n";

    asio::co_spawn(io_context_,
                   [this, socket, token]() -> asio::awaitable<void> {
                       co_await do_read(socket, token);
                   },
                   asio::detached);
}

template<typename CancellationToken>
asio::awaitable<std::string>
co_read_headers(asio::ip::tcp::socket &socket,
                asio::streambuf &buffer,
                CancellationToken token) {
    const std::size_t header_bytes = co_await asio::async_read_until(
        socket, buffer, "\r\n\r\n",
        asio::bind_cancellation_slot(token, asio::use_awaitable));

    co_return take_front(buffer, header_bytes);
}

template<typename CancellationToken>
asio::awaitable<std::string>
co_read_body(asio::ip::tcp::socket &socket,
             asio::streambuf &buffer,
             std::size_t content_len,
             CancellationToken token,
             const ProgressCallback &on_progress = {}) {
    std::string body;
    body.reserve(content_len);

    {
        const std::size_t avail = buffer.size();
        const std::size_t take = std::min(avail, content_len);
        if (take) {
            body += take_front(buffer, take);
            if (on_progress) on_progress(body.size(), take);
        }
    }

    while (body.size() < content_len) {
        const std::size_t need = content_len - body.size();

        std::size_t n = co_await asio::async_read(
            socket, buffer,
            asio::transfer_exactly(need),
            asio::bind_cancellation_slot(token, asio::use_awaitable));

        body += take_front(buffer, n);
        if (on_progress) on_progress(body.size(), n);
    }

    co_return body;
}


template<typename CancellationToken>
asio::awaitable<void> Server::do_read(std::shared_ptr<asio::ip::tcp::socket> socket, CancellationToken token,
                                      const ProgressCallback &on_progress) {
    asio::streambuf buffer;

    while (true) {
        auto cs = co_await asio::this_coro::cancellation_state;
        if (cs.cancelled() != asio::cancellation_type::none) {
            throw asio::system_error(asio::error::operation_aborted);
        }

        std::string headers_text = co_await co_read_headers(*socket, buffer, token);

        Request req;
        parse_http_request(headers_text, req);


        const std::size_t content_len = req.content_length().value_or(0);
        if (content_len) {
            std::string body = co_await co_read_body(*socket, buffer, content_len, token, on_progress);
            req.body = std::move(body);
        }

        std::cout << req.to_string() << "\n";


        Response resp = Response::text("OK\n");
        std::string payload = resp.to_string();
        co_await do_write(socket, payload, token);
    }
}

template<typename CancellationToken>
asio::awaitable<void> Server::do_write(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string &data,
                                       CancellationToken token) {
    co_await asio::async_write(*socket,
                               asio::buffer(data),
                               asio::bind_cancellation_slot(token, asio::use_awaitable));
}

int main() {
    asio::io_context io_context;

    unsigned short port = 12345;
    Server server(io_context, port);

    server.start();
    std::cout << "Server started on port " << port << "\n";

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}
