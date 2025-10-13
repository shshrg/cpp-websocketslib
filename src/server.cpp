#include "server.h"
#include <iostream>
#include <thread>
#include "http/request.h"


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

    cancel_all_sessions();

    if (work_guard_)
        work_guard_.reset();

    for (auto &t: workers_)
        if (t.joinable()) t.join();

    workers_.clear();

}

asio::awaitable<void> Server::do_accept() {
    auto ex = co_await asio::this_coro::executor;
    std::atomic_int counter = 0;

    for (;;) {
        ++counter;
        auto socket = std::make_shared<tcp::socket>(ex);
        auto [ec] = co_await acceptor_.async_accept(*socket,
        asio::bind_cancellation_slot(server_slot_, asio::as_tuple(asio::use_awaitable)));

        if (ec == asio::error::operation_aborted)
            co_return;

        if (ec) continue;

        std::cout << "New client connected: " << counter << "\n";

        auto session = std::make_shared<Session>(socket);
        add_session(session);

        asio::co_spawn(io_context_,
                   [this, s = session]() -> asio::awaitable<void> {
                       co_await s->run_once();
                       remove_session(s);
                       co_return;
                   },
                   asio::detached);
    }

}

void Server::add_session(const std::shared_ptr<Session>& s) {
    std::scoped_lock lk(mutex_);
    sessions_.insert(s);
}

void Server::remove_session(const std::shared_ptr<Session>& s) {
    std::scoped_lock lk(mutex_);
    sessions_.erase(s);
}

void Server::cancel_all_sessions() {
    std::vector<std::shared_ptr<Session>> copy;
    {
        std::scoped_lock lk(mutex_);
        copy.assign(sessions_.begin(), sessions_.end());
    }
    for (auto& s : copy) s->cancel();
}