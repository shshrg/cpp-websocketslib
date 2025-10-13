#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#include <asio.hpp>
#include <memory>
#include <vector>

typedef std::function<void(size_t, size_t)> ProgressCallback;

class Server : public std::enable_shared_from_this<Server> {
public:
    Server(asio::io_context &io_context, const asio::ip::address &address, unsigned short port);

    void start(size_t worker_threads);
    void stop();
    void cancel_all();

    bool is_active() {
        std::lock_guard lock(mutex_);
        return is_running_;
    }

private:
    template <typename CancellationToken>
    asio::awaitable<void> do_accept(CancellationToken token);

    template <typename CancellationToken>
    asio::awaitable<void> do_read(std::shared_ptr<asio::ip::tcp::socket> socket, CancellationToken token, const ProgressCallback& on_progress = nullptr);

    template <typename CancellationToken>
    asio::awaitable<void> do_write(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& data, CancellationToken token);

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets_;
    asio::cancellation_signal cancellation_signal_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    bool is_running_ = false;

    static constexpr size_t bufferSize = 1024;
};
#endif //ASIO_CANCEL_SERVER_H