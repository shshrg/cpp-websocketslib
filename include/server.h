#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#include <asio.hpp>
#include <memory>
#include <shared_mutex>
#include <vector>
#include "http/request.h"


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
    // Client cancellation
    asio::cancellation_slot get_client_slot(size_t client_id);
    asio::awaitable<void> handle_client(tcp::socket socket, size_t client_id);
    void add_client(size_t client_id);
    void emit_client(size_t client_id);
    void emit_all();
    void remove_client(size_t client_id);


    asio::awaitable<void> do_accept();
    asio::awaitable<Request> do_read(tcp::socket & socket, asio::cancellation_slot token);
    asio::awaitable<void> do_write(tcp::socket & socket, asio::cancellation_slot token);

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    std::optional<asio::executor_work_guard<asio::io_context::executor_type> > work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::shared_mutex mutex_;
    std::unordered_map<size_t, asio::cancellation_signal> client_cancel_;
};


#endif //ASIO_CANCEL_SERVER_H
