#ifndef ASIO_CANCEL_SERVER_H
#define ASIO_CANCEL_SERVER_H

/**
 * @file server.h
 * @brief Asynchronous HTTP / WebSocket server based on standalone Asio.
 *
 * Provides:
 *  - HTTP routing with sync and async handlers
 *  - WebSocket routing
 *  - Static file serving
 *  - TLS (OpenSSL) support
 *  - Per-client and global cancellation
 */

#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif
#include <asio.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <filesystem>
#include <type_traits>

#include "http/request.h"
#include "http/response.h"
#include "websocket/WebSocket.h"

/**
 * @brief Unified asynchronous HTTP handler type.
 *
 * All handlers (sync and async) are converted to this type internally.
 */
using HandlerAsync = std::function<asio::awaitable<Response>(const Request &)>;

/**
 * @brief Synchronous HTTP handler type.
 */
using HandlerSync = std::function<Response(const Request &)>;

namespace fs = std::filesystem;

/**
 * @brief Helper alias to extract handler return type.
 */
template<class F>
using call_result_t = std::invoke_result_t<F, const Request &>;

/**
 * @brief Constraint for synchronous handlers.
 *
 * Handler must return Response.
 */
template<class F>
concept SyncConstraint = std::is_same_v<call_result_t<F>, Response>;

/**
 * @brief Constraint for asynchronous handlers.
 *
 * Handler must return asio::awaitable<Response>.
 */
template<class F>
concept AsyncConstraint = std::is_same_v<call_result_t<F>, asio::awaitable<Response> >;

/**
 * @brief Comparator for static mount prefixes.
 *
 * Longer prefixes have higher priority.
 */
struct PrefixComparator {
    bool operator()(const std::string &a, const std::string &b) const {
        if (a.size() != b.size())
            return a.size() > b.size();
        return a < b;
    }
};

/**
 * @brief Main asynchronous HTTP / WebSocket server.
 *
 * Features:
 *  - Coroutine-based request handling
 *  - HTTP routing (GET/POST/PUT/DELETE)
 *  - WebSocket routing
 *  - Static file serving
 *  - TLS support (optional)
 *  - Graceful shutdown and cancellation
 */
class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;

    /**
     * @brief Construct server instance.
     *
     * @param io_context Shared Asio io_context
     * @param address Bind address
     * @param port Bind port
     * @param use_ssl Enable TLS (requires OpenSSL)
     */
    explicit Server(asio::io_context &io_context,
                    const asio::ip::address &address,
                    unsigned short port,
                    bool use_ssl = false);

    /// Register HTTP GET handler
    template<typename F>
    void Get(std::string path, F h) { add_route(Method::GET, std::move(path), std::move(h)); }

    /// Register HTTP POST handler
    template<typename F>
    void Post(std::string path, F h) { add_route(Method::POST, std::move(path), std::move(h)); }

    /// Register HTTP PUT handler
    template<typename F>
    void Put(std::string path, F h) { add_route(Method::PUT, std::move(path), std::move(h)); }

    /// Register HTTP DELETE handler
    template<typename F>
    void Delete(std::string path, F h) { add_route(Method::DELETE_, std::move(path), std::move(h)); }

    /**
     * @brief Mount static directory.
     *
     * @param url_prefix URL prefix (e.g. "/static")
     * @param root Filesystem path
     */
    void MountStatic(std::string url_prefix, fs::path root);

    /**
     * @brief Start server worker threads.
     *
     * @param worker_threads Maximum number of threads
     */
    void start(size_t worker_threads);

    /**
     * @brief Gracefully stop server.
     */
    void stop();

    /**
     * @brief Post task to server io_context.
     */
    void post_task(std::function<void()> task);

    /**
     * @brief WebSocket route builder.
     *
     * Uses RAII — route is committed on destruction.
     */
    class WsRouter {
    public:
        /// Set WebSocket open handler
        WsRouter &on_open(WsOpenHandler h);

        /// Set WebSocket message handler
        WsRouter &on_message(WsMessageHandler h);

        /// Set WebSocket close handler
        WsRouter &on_close(WsCloseHandler h);

        /// Commit route on destruction
        ~WsRouter();

    private:
        friend class Server;
        WsRouter(Server &s, std::string p);

        Server &srv_;
        std::string path_;
        WsHandlers handlers_{};
    };

    /**
     * @brief Create WebSocket route.
     *
     * @param path WebSocket URL path
     */
    WsRouter WebSocketRouter(std::string path);

    /**
     * @brief Find WebSocket handlers for path.
     */
    const WsHandlers *find_ws(const std::string &path) const;

private:
    /* ===================== Client lifecycle ===================== */

    asio::cancellation_slot get_client_slot(size_t client_id);
    asio::awaitable<void> handle_client(tcp::socket socket, size_t client_id);

    void add_client(size_t client_id);
    void emit_client(size_t client_id);
    void emit_all();
    void remove_client(size_t client_id);

    /* ===================== SSL ===================== */

    bool setup_ssl();

    /* ===================== Routing ===================== */

    template<typename F>
    void add_route(Method method, std::string path, F h) {
        routes_[method].emplace(std::move(path), make_async(std::move(h)));
    }

    /**
     * @brief Convert sync handler to async.
     */
    template<SyncConstraint F>
    HandlerAsync make_async(F h) {
        return [fn = std::move(h)](const Request &req) -> asio::awaitable<Response> {
            co_return fn(req);
        };
    }

    /**
     * @brief Forward async handler.
     */
    template<AsyncConstraint F>
    HandlerAsync make_async(F h) {
        return [fn = std::move(h)](const Request &req) -> asio::awaitable<Response> {
            co_return co_await fn(req);
        };
    }

    /* ===================== WebSockets ===================== */

    std::unordered_map<size_t, std::shared_ptr<WebSocket>> websockets_;
    std::mutex ws_mutex;

    void register_websocket(size_t id, std::shared_ptr<WebSocket> ws);
    void remove_websocket(size_t id);
    void close_websockets();

    /* ===================== HTTP processing ===================== */

    asio::awaitable<Response> serve_static(const Request &req);
    asio::awaitable<Response> handle_request(const Request &request);

    asio::awaitable<void> do_accept();

    template<typename Socket>
    asio::awaitable<Request> do_read(Socket &socket, asio::cancellation_slot token);

    template<typename Socket>
    asio::awaitable<void> do_write(Socket &socket, Response &response, asio::cancellation_slot token);

    template<typename Socket>
    asio::awaitable<void> process_session(Socket &socket,
                                          asio::cancellation_slot token,
                                          size_t client_id);

    template<typename Socket>
    asio::awaitable<void> process_session_ws(Socket socket,
                                             const std::string &sec_ws_key,
                                             const WsHandlers *handlers,
                                             asio::cancellation_slot token,
                                             size_t client_id);

    /* ===================== Internal state ===================== */

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

#ifdef USE_SSL
    bool use_ssl_;
    asio::ssl::context ssl_context_;
#else
    bool use_ssl_ = false;
#endif

    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::vector<std::thread> workers_;

    asio::cancellation_signal server_cancel_;
    asio::cancellation_slot server_slot_ = server_cancel_.slot();

    std::mutex mutex_;
    std::unordered_map<size_t, asio::cancellation_signal> client_cancel_;

    std::unordered_map<Method,
        std::unordered_map<std::string, HandlerAsync>> routes_;

    /// Static file mounts
    std::map<std::string, fs::path, PrefixComparator> static_mounts_;

    /// WebSocket routes
    std::unordered_map<std::string, WsHandlers> ws_routes_;
};

#endif // ASIO_CANCEL_SERVER_H
