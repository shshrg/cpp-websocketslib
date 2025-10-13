#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#include <asio.hpp>
#include <memory>
#include <vector>
#include <unordered_set>
#include "session.h"


class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;

    explicit Server(asio::io_context &io_context, const asio::ip::address &address, unsigned short port)
        : io_context_(io_context),
          acceptor_(io_context_, tcp::endpoint(address, port)) {
    }

    void start(size_t worker_threads);
    void stop();

private:
    asio::awaitable<void> do_accept();

    void add_session(const std::shared_ptr<Session> &s);
    void remove_session(const std::shared_ptr<Session> &s);
    void cancel_all_sessions();

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    std::optional<asio::executor_work_guard<asio::io_context::executor_type> > work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::mutex mutex_;
    std::unordered_set<std::shared_ptr<Session> > sessions_;
};


#endif //ASIO_CANCEL_SERVER_H
