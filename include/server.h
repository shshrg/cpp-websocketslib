#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
#include <asio.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <filesystem>
#include "http/request.h"
#include "http/response.h"
#include "websocket/WebSocket.h"

using HandlerAsync = std::function<asio::awaitable<Response>(const Request &)>;
using HandlerSync = std::function<Response(const Request &)>;

namespace fs = std::filesystem;

// Just check the return type of handler
template<class F>
using call_result_t = std::invoke_result_t<F, const Request &>;

// concept is a set of requirements
// i.e. some constraint onto the macro to distinguish between SyncHandler and AsyncHandler
template<class F>
concept SyncConstraint = std::is_same_v<call_result_t<F>, Response>;

// Need a constraint here to differ between make_async in add_route
template<class F>
concept AsyncConstraint = std::is_same_v<call_result_t<F>, asio::awaitable<Response> >;

struct PrefixComparator {
    bool operator()(const std::string &a, const std::string &b) const {
        if (a.size() != b.size())
            return a.size() > b.size();
        return a < b;
    }
};


class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;

    explicit Server(asio::io_context &io_context, const asio::ip::address &address, unsigned short port,
                    bool use_ssl = false)
    : io_context_(io_context),
    acceptor_(io_context_, tcp::endpoint(address, port))
    #ifdef USE_SSL
    , use_ssl_(use_ssl), ssl_context_(asio::ssl::context::tls_server)
    #else
    , use_ssl_(false)
    #endif
    {
    #ifdef USE_SSL
    if (use_ssl_ && !setup_ssl())
        throw std::runtime_error("SSL setup failed");
    #else
    if (use_ssl)
        std::cerr << "WARNING: SSL requested but OpenSSL is disabled!\n";
    #endif
    }

    // Method Interfaces
    template<typename F>
    void Get(std::string path, F h) { add_route(Method::GET, std::move(path), std::move(h)); }

    template<typename F>
    void Post(std::string path, F h) { add_route(Method::POST, std::move(path), std::move(h)); }

    template<typename F>
    void Put(std::string path, F h) { add_route(Method::PUT, std::move(path), std::move(h)); }

    template<typename F>
    void Delete(std::string path, F h) { add_route(Method::DELETE_, std::move(path), std::move(h)); }

    // Mount Static
    void MountStatic(std::string url_prefix, fs::path root);


    void start(size_t worker_threads);
    void stop();

    void post_task(std::function<void()> task);

    class WsRouter {
    public:
        WsRouter &on_open(WsOpenHandler h) {
            handlers_.on_open = std::move(h);
            return *this;
        }

        WsRouter &on_message(WsMessageHandler h) {
            handlers_.on_message = std::move(h);
            return *this;
        }

        WsRouter &on_close(WsCloseHandler h) {
            handlers_.on_close = std::move(h);
            return *this;
        }

        ~WsRouter() { srv_.commit_ws_route(std::move(path_), std::move(handlers_)); }

    private:
        friend class Server;

        WsRouter(Server &s, std::string p) : srv_(s), path_(std::move(p)) {
        }

        Server &srv_;
        std::string path_;
        WsHandlers handlers_{};
    };

    WsRouter WebSocketRouter(std::string path) { return WsRouter{*this, std::move(path)}; }

    const WsHandlers *find_ws(const std::string &path) const {
        auto it = ws_routes_.find(path);
        return it == ws_routes_.end() ? nullptr : &it->second;
    }

private:
    // Client handling
    asio::cancellation_slot get_client_slot(size_t client_id);
    asio::awaitable<void> handle_client(tcp::socket socket, size_t client_id);
    void add_client(size_t client_id);
    void emit_client(size_t client_id);
    void emit_all();
    void remove_client(size_t client_id);

    //ssl
    bool setup_ssl();

    // Routing
    template<typename F>
    void add_route(Method method, std::string path, F h) {
        routes_[method].emplace(std::move(path), make_async(std::move(h)));
    }


    // Convert to awaitable
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
    // keep track of websockets for all connections
    std::unordered_map<size_t, std::shared_ptr<WebSocket>> websockets_;
    std::mutex ws_mutex;
    void register_websocket(size_t id, std::shared_ptr<WebSocket> ws)
    {
        std::lock_guard lock(ws_mutex);
        websockets_[id] = ws;
    }
    void remove_websocket(size_t id)
    {
        std::lock_guard lock(ws_mutex);
        websockets_.erase(id);
    }

    asio::awaitable<Response> serve_static(const Request &req);

    asio::awaitable<Response> handle_request(const Request &request);

    template<typename Socket>
    asio::awaitable<void> process_session(Socket &socket, asio::cancellation_slot token, size_t client_id);


    asio::awaitable<void> do_accept();
    template<typename Socket>
    asio::awaitable<Request> do_read(Socket &socket, asio::cancellation_slot token);
    template<typename Socket>
    asio::awaitable<void> do_write(Socket &socket, const Response &response, asio::cancellation_slot token);

    void commit_ws_route(std::string path, WsHandlers handlers) { ws_routes_[std::move(path)] = std::move(handlers); }

    template<typename Socket>
    asio::awaitable<void> process_session_ws(Socket &socket, const std::string &sec_ws_key,
                                             const WsHandlers *handlers, asio::cancellation_slot token, size_t client_id);

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    #ifdef USE_SSL
        bool use_ssl_;
        asio::ssl::context ssl_context_;
    #else
        bool use_ssl_ = false;
    #endif

    std::optional<asio::executor_work_guard<asio::io_context::executor_type> > work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::mutex mutex_;
    std::unordered_map<size_t, asio::cancellation_signal> client_cancel_;

    std::unordered_map<Method, std::unordered_map<std::string, HandlerAsync> > routes_;

    // Serve Static
    std::map<std::string, fs::path, PrefixComparator> static_mounts_;

    std::unordered_map<std::string, WsHandlers> ws_routes_;
};


#endif //ASIO_CANCEL_SERVER_H