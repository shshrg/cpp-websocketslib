#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <vector>
#include "http/request.h"
#include "http/response.h"

using Handler = std::function<asio::awaitable<Response>(const Request &)>;


class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;

    explicit Server(asio::io_context &io_context, const asio::ip::address &address, unsigned short port,
        bool use_ssl = false)
        : io_context_(io_context),
          acceptor_(io_context_, tcp::endpoint(address, port)),
          use_ssl_(use_ssl), ssl_context_(asio::ssl::context::tls_server) {
        if (use_ssl_)
        {
            ssl_context_.set_options(
            asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::single_dh_use);

            ssl_context_.use_certificate_chain_file("certs/cert.pem");
            ssl_context_.use_private_key_file("certs/key.pem", asio::ssl::context::pem);
            ssl_context_.use_tmp_dh_file("certs/dhparam.pem");
        }
    }

    // Method Interfaces
    void Get(std::string path, Handler handler) { add_route(Method::GET, std::move(path), std::move(handler)); }
    void Post(std::string path, Handler handler) { add_route(Method::POST, std::move(path), std::move(handler)); }
    void Put(std::string path, Handler handler) { add_route(Method::PUT, std::move(path), std::move(handler)); }
    void Delete(std::string path, Handler handler) { add_route(Method::DELETE, std::move(path), std::move(handler)); }


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
    void add_route(Method method, std::string path, Handler handler);
    asio::awaitable<Response> handle_request(const Request& request);

    template<typename Socket>
    asio::awaitable<void> process_session(Socket &socket, asio::cancellation_slot token);


    asio::awaitable<void> do_accept();
    template <typename Socket>
    asio::awaitable<Request> do_read(Socket &socket, asio::cancellation_slot token);
    template <typename Socket>
    asio::awaitable<void> do_write(Socket &socket, const Response & response, asio::cancellation_slot token);

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    bool use_ssl_;
    asio::ssl::context ssl_context_;

    std::optional<asio::executor_work_guard<asio::io_context::executor_type> > work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::mutex mutex_;
    std::unordered_map<size_t, asio::cancellation_signal> client_cancel_;

    std::unordered_map<Method, std::unordered_map<std::string, Handler>> routes_;
};


#endif //ASIO_CANCEL_SERVER_H
