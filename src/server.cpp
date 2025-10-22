#include "server.h"
#include <iostream>
#include <thread>
#include "http/request.h"
#include "http/response.h"
#include "http/utils.h"
#include "io_helpers.h"


void Server::start(size_t worker_threads) {
    if (work_guard_) return;
    work_guard_.emplace(asio::make_work_guard(io_context_));

    asio::co_spawn(io_context_, do_accept(), asio::detached);

    size_t threads = std::min(worker_threads, static_cast<size_t>(std::thread::hardware_concurrency()));

    workers_.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { io_context_.run(); });
    }
}

void Server::stop() {
    server_cancel_.emit(asio::cancellation_type::all);

    emit_all();

    if (work_guard_)
        work_guard_.reset();

    for (auto &t: workers_)
        if (t.joinable()) t.join();

    workers_.clear();

}

asio::awaitable<void> Server::do_accept() {
    size_t client_id = 0;

    while (true) {
        ++client_id;
        tcp::socket socket = co_await acceptor_.async_accept(asio::bind_cancellation_slot(server_slot_, asio::use_awaitable));

        std::cout << "New client connected: " << client_id << "\n";


        asio::co_spawn(io_context_,
                   [this, &socket, client_id]() -> asio::awaitable<void> {
                       co_await handle_client(std::move(socket), client_id);
                       remove_client(client_id);
                       co_return;
                   },
                   asio::detached);
    }

}


asio::awaitable<void> Server::handle_client(tcp::socket socket, size_t client_id) {

    asio::cancellation_slot token = client_cancel_[client_id].slot();
    Request req = co_await do_read(socket, token);

    std::cout << req.to_string() << "\n";

    co_await do_write(socket, token);
}


asio::awaitable<Request> Server::do_read(tcp::socket & socket, asio::cancellation_slot token) {
    Request req;
    asio::streambuf buf;

    std::string headers_text = co_await co_read_headers(socket, buf, token);

    parse_http_request(headers_text, req); // TODO: make coroutine

    const std::size_t clen = req.content_length().value_or(0);
    if (clen) {
        req.body = co_await co_read_body(socket, buf, clen, token);
    }

    co_return req;
}


asio::awaitable<void> Server::do_write(tcp::socket & socket, asio::cancellation_slot token) {
    Response resp = Response::text("OK\n");
    std::string payload = resp.to_string();

    co_await asio::async_write(socket, asio::buffer(payload), asio::bind_cancellation_slot(token, asio::use_awaitable));
}

void Server::emit_client(size_t client_id) {
    client_cancel_[client_id].emit(asio::cancellation_type::all);
}

void Server::emit_all() {
    for (const auto &pair: client_cancel_) {
        emit_client(pair.first);
    }
}

void Server::remove_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    client_cancel_.erase(client_id);
}
