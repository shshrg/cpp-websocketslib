#include "server.h"
#include <iostream>
#include <ranges>
#include <thread>
#include "http/utils.h"
#include "io_helpers.h"


void Server::start(size_t worker_threads) {
    if (work_guard_) return;

    // Keep io_context alive
    work_guard_.emplace(asio::make_work_guard(io_context_));

    asio::co_spawn(io_context_, do_accept(), asio::detached);

    size_t threads = std::min(worker_threads, static_cast<size_t>(std::thread::hardware_concurrency()));

    workers_.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { io_context_.run(); });
    }
}

void Server::stop() {
    // Stop accepting clients
    server_cancel_.emit(asio::cancellation_type::all);

    // Finish read/write client operations
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
        tcp::socket socket = co_await acceptor_.async_accept(
            asio::bind_cancellation_slot(server_slot_, asio::use_awaitable));

        std::cout << "New client connected: " << client_id << "\n";

        // Add cancellation signal bind to the client
        add_client(client_id);

        asio::co_spawn(io_context_,
                       [this, &socket, client_id]() -> asio::awaitable<void> {
                           co_await handle_client(std::move(socket), client_id);
                           remove_client(client_id);
                           co_return;
                       },
                       asio::detached);
    }
}

template <typename Socket>
asio::awaitable<void> Server::process_session(Socket &socket, asio::cancellation_slot token)
{
    Request req = co_await do_read(socket, token);
    std::cout << req.to_string() << "\n";

    Response resp = co_await handle_request(req);
    co_await do_write(socket, resp, token);
}


asio::awaitable<void> Server::handle_client(tcp::socket socket, size_t client_id) {
    asio::cancellation_slot token = get_client_slot(client_id);

    if (use_ssl_)
    {
        std::cout << "Client " << client_id << " use TLS\n";
        asio::ssl::stream<tcp::socket> ssl_stream(std::move(socket), ssl_context_);
        co_await ssl_stream.async_handshake(asio::ssl::stream_base::server,
            asio::bind_cancellation_slot(token, asio::use_awaitable));
        co_await process_session(ssl_stream, token);
    } else
    {
        co_await process_session(socket, token);
    }
}

template <typename Socket>
asio::awaitable<Request> Server::do_read(Socket &socket, asio::cancellation_slot token) {
    Request req;
    asio::streambuf buf;

    std::string headers_text = co_await co_read_headers(socket, buf, token);

    parse_http_request(headers_text, req);

    const std::size_t clen = req.content_length().value_or(0);
    if (clen) {
        req.body = co_await co_read_body(socket, buf, clen, token);
    }

    co_return req;
}

template <typename Socket>
asio::awaitable<void> Server::do_write(Socket &socket, const Response &response, asio::cancellation_slot token) {
    std::string payload = response.to_string();

    co_await asio::async_write(socket, asio::buffer(payload), asio::bind_cancellation_slot(token, asio::use_awaitable));
}

void Server::add_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    client_cancel_.try_emplace(client_id);
}

asio::cancellation_slot Server::get_client_slot(size_t client_id) {
    std::lock_guard lock(mutex_);
    auto it = client_cancel_.find(client_id);
    return (it != client_cancel_.end()) ? it->second.slot() : asio::cancellation_slot();
}


void Server::emit_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    auto it = client_cancel_.find(client_id);
    if (it != client_cancel_.end())
        it->second.emit(asio::cancellation_type::all);
}

void Server::emit_all() {
    std::vector<size_t> client_ids;
    {
        std::lock_guard lock(mutex_);
        client_ids.reserve(client_cancel_.size());
        for (const auto &key: client_cancel_ | std::views::keys) {
            client_ids.push_back(key);
        }
    }
    for (size_t id: client_ids)
        emit_client(id);
}

void Server::remove_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    client_cancel_.erase(client_id);
}


void Server::add_route(Method method, std::string path, Handler handler) {
    routes_[method].emplace(std::move(path), std::move(handler));
}


asio::awaitable<Response> Server::handle_request(const Request &request) {
    auto it_method = routes_.find(request.method);
    if (it_method != routes_.end()) {
        auto it_path = it_method->second.find(request.path);
        if (it_path != it_method->second.end()) {
            co_return co_await it_path->second(request);
        }
    }
    co_return Response::not_found();
}
