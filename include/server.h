#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#include <asio.hpp>
#include <memory>
#include <utility>
#include <vector>
#include "http/request.h"
#include "http/response.h"

using HandlerAsync = std::function<asio::awaitable<Response>(const Request &)>;
using HandlerSync = std::function<Response(const Request &)>;

// Just check the return type of handler
template<class F>
using call_result_t = std::invoke_result_t<F, const Request &>;

// concept is a set of requirements
// i.e. some constraint onto the macro to distinguish between SyncHandler and AsyncHandler
template<class F>
concept SyncConstraint = std::is_same_v<call_result_t<F>, Response>;

// Need a constraint here to differ between make_async in add_route
template<class F>
concept AsyncConstraint = std::is_same_v<call_result_t<F>, asio::awaitable<Response>>;


class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;

    explicit Server(asio::io_context &io_context, const asio::ip::address &address, unsigned short port)
        : io_context_(io_context),
          acceptor_(io_context_, tcp::endpoint(address, port)) {
    }

    // Method Interfaces
    template<typename F>
    void Get(std::string path, F h) { add_route(Method::GET, std::move(path), std::move(h)); }

    template<typename F>
    void Post(std::string path, F h) { add_route(Method::POST, std::move(path), std::move(h)); }

    template<typename F>
    void Put(std::string path, F h) { add_route(Method::PUT, std::move(path), std::move(h)); }

    template<typename F>
    void Delete(std::string path, F h) { add_route(Method::DELETE, std::move(path), std::move(h)); }

    void start(size_t worker_threads);
    void stop();

private:
    // Client handling
    asio::cancellation_slot get_client_slot(size_t client_id);
    asio::awaitable<void> handle_client(tcp::socket socket, size_t client_id);
    void add_client(size_t client_id);
    void emit_client(size_t client_id);
    void emit_all();
    void remove_client(size_t client_id);

    // Routing
    template<typename F>
    void add_route(Method method, std::string path, F h) {
        routes_[method].emplace(std::move(path), make_async(std::move(h)));
    }


    template<SyncConstraint F>
    HandlerAsync make_async(F h) {
        return [fn = std::move(h)](const Request &req) -> asio::awaitable<Response> {
            co_return fn(req);
        };
    }

    template<AsyncConstraint F>
    HandlerAsync make_async(F h) {
        return [fn = std::move(h)](const Request &req) -> asio::awaitable<Response> {
            co_return co_await fn(req);
        };
    }

    asio::awaitable<Response> handle_request(const Request &request);


    asio::awaitable<void> do_accept();
    asio::awaitable<Request> do_read(tcp::socket &socket, asio::cancellation_slot token);
    asio::awaitable<void> do_write(tcp::socket &socket, const Response &response, asio::cancellation_slot token);

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    std::optional<asio::executor_work_guard<asio::io_context::executor_type> > work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::mutex mutex_;
    std::unordered_map<size_t, asio::cancellation_signal> client_cancel_;

    std::unordered_map<Method, std::unordered_map<std::string, HandlerAsync> > routes_;
};


#endif //ASIO_CANCEL_SERVER_H
