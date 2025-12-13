/**
 * @file server.cpp
 * @brief Implementation of the high-performance ASIO-based HTTP/WebSocket server.
 *
 * This file contains the core implementation of the Server class, handling TCP/SSL connections,
 * HTTP request processing, WebSocket upgrades, static file serving with platform-optimized
 * zero-copy transfers (sendfile/TransmitFile), and multi-threaded I/O context management.
 * Supports keep-alive, cancellation signals for graceful shutdown, and route-based request handling.
 *
 * @version 1.0
 * @date 2025
 */

#include "server.h"
#include <iostream>
#include <ranges>
#include <thread>
#include "http/utils.h"
#include "io_helpers.h"
#include <asio/stream_file.hpp>
#ifdef __linux__
#include <csignal>
#include <sys/sendfile.h>
#endif

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#endif

/**
 * @brief Starts the server with the specified number of worker threads.
 *
 * Initializes the I/O context work guard to prevent premature shutdown, ignores SIGPIPE on Linux
 * for robustness, spawns the accept loop, and launches worker threads to run the io_context_.
 * The number of threads is capped at the hardware concurrency level.
 *
 * @param worker_threads Number of worker threads to spawn. Defaults to hardware concurrency if higher.
 */
void Server::start(size_t worker_threads) {
    if (work_guard_) return;
#ifdef __linux__
    signal(SIGPIPE, SIG_IGN);
#endif

    // Keep io_context alive
    work_guard_.emplace(asio::make_work_guard(io_context_));

    asio::co_spawn(io_context_, do_accept(), asio::detached);

    size_t threads = std::min(worker_threads, static_cast<size_t>(std::thread::hardware_concurrency()));

    workers_.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { io_context_.run(); });
    }
}

/**
 * @brief Gracefully stops the server.
 *
 * Cancels all ongoing accepts and client operations via cancellation signals, closes WebSockets,
 * resets the work guard, and joins all worker threads. Ensures clean shutdown without abrupt closes.
 */
void Server::stop() {
    // Stop accepting clients
    server_cancel_.emit(asio::cancellation_type::all);

    emit_all();

    close_websockets();

    if (work_guard_)
        work_guard_.reset();

    for (auto &t: workers_)
        if (t.joinable()) t.join();

    workers_.clear();
}

/**
 * @brief Asynchronously accepts incoming client connections in a loop.
 *
 * Runs indefinitely until cancelled (e.g., via server shutdown). For each accepted socket,
 * spawns a new handle_client coroutine. Uses client_id for tracking.
 *
 * @return asio::awaitable<void> Coroutine that completes on cancellation.
 */
asio::awaitable<void> Server::do_accept() {
    size_t client_id = 0;

    while (true) {
        ++client_id;
        auto [ec, socket] = co_await acceptor_.async_accept(
            asio::as_tuple(asio::bind_cancellation_slot(server_slot_, asio::use_awaitable)));

        if (ec == asio::error::operation_aborted)
            co_return;

        // std::cout << "New client connected: " << client_id << "\n";

        asio::co_spawn(io_context_,
                       handle_client(std::move(socket), client_id),
                       asio::detached);
    }
}

/**
 * @brief Processes an HTTP session over the given socket, handling multiple requests via keep-alive.
 *
 * Reads requests in a loop, checks for WebSocket upgrades, routes HTTP requests, and writes responses.
 * Supports Connection: keep-alive/close header parsing for persistent connections.
 *
 * @tparam Socket The socket type (tcp::socket or ssl::stream<tcp::socket>).
 * @param socket Reference to the connected socket.
 * @param token Cancellation slot for aborting operations.
 * @param client_id Unique ID for this client session.
 * @return asio::awaitable<void> Coroutine that exits on error, close, or cancellation.
 */
template<typename Socket>
asio::awaitable<void> Server::process_session(Socket &socket, asio::cancellation_slot token, size_t client_id) {
    while (true) {
        Request req;
        try {
            req = co_await do_read(socket, token);
        } catch (const std::exception &e) {
            break;
        }

        if (req.is_ws_upgrade()) {
            // std::cout << "It is an upgrade!" << std::endl;
            const auto *handlers = find_ws(req.path);
            if (!handlers) {
                Response resp = Response::not_found("No such route for ws!");
                co_await asio::async_write(socket, asio::buffer(resp.to_string()),
                                           asio::bind_cancellation_slot(token, asio::use_awaitable));
                co_return;
            }
            auto key = req.get_header_value("Sec-WebSocket-Key");
            if (key.empty()) {
                Response resp = Response::bad_request("No Sec-WebSocket-Key");
                co_await asio::async_write(socket, asio::buffer(resp.to_string()),
                                           asio::bind_cancellation_slot(token, asio::use_awaitable));
                co_return;
            }
            co_await process_session_ws(std::move(socket), key, handlers, token, client_id);
            co_return;
        }

        Response resp = co_await handle_request(req);

        bool keep_alive = true;
        auto conn = req.get_header_value("Connection");
        if (!conn.empty()) {
            if (conn == "close") keep_alive = false;
        }
        if (!keep_alive) {
            resp.set_header("Connection", "close");
        } else {
            resp.set_header("Connection", "keep-alive");
        }
        co_await do_write(socket, resp, token);

        if (!keep_alive) {
            break;
        }
    }
}

/**
 * @brief Handles WebSocket upgrade and session after HTTP 101 response.
 *
 * Computes Sec-WebSocket-Accept key, sends 101 Switching Protocols response, creates WebSocket
 * instance, registers it, sets up cleanup callbacks, and starts the WS loop.
 *
 * @tparam Socket The socket type.
 * @param socket Moved socket for WebSocket ownership.
 * @param sec_ws_key The Sec-WebSocket-Key from HTTP upgrade request.
 * @param handlers WebSocket event handlers for this path.
 * @param token Cancellation slot.
 * @param client_id Client ID.
 * @return asio::awaitable<void> Coroutine for WS session.
 */
template<typename Socket>
asio::awaitable<void> Server::process_session_ws(Socket socket,
                                                 const std::string &sec_ws_key,
                                                 const WsHandlers *handlers,
                                                 asio::cancellation_slot token,
                                                 size_t client_id) {
    std::string accept = ws_accept_key(sec_ws_key);
    Response resp = build_101_response(accept);
    co_await do_write(socket, resp, token);

    auto ws = std::make_shared<WebSocket>(
        std::move(socket), handlers, client_id);

    ws->set_server_cleanup(
        [this](size_t id) {
            remove_websocket(id);
        }
    );

    register_websocket(client_id, ws);

    if (token.is_connected()) {
        std::weak_ptr<WebSocket> weak_ws = ws;
        token.assign([weak_ws](asio::cancellation_type type) {
            if (auto s = weak_ws.lock()) {
                s->close_async(1001, "Cancelled by server");
            }
        });
    }

    co_await ws->start();
}

/**
 * @brief Routes and handles an incoming HTTP request.
 *
 * Looks up route handlers by method and path, falls back to static file serving if no route found.
 *
 * @param req The parsed HTTP Request.
 * @return asio::awaitable<Response> The generated Response.
 */
asio::awaitable<Response> Server::handle_request(const Request &req) {
    auto method_it = routes_.find(req.method);
    if (method_it != routes_.end()) {
        auto &table = method_it->second;
        if (auto it = table.find(req.path); it != table.end()) {
            co_return co_await it->second(req);
        }
    }

    if (!static_mounts_.empty()) {
        Response res = co_await serve_static(req);
        co_return res;
    }
    co_return Response::not_found();
}

/**
 * @brief Handles a full client connection, including optional SSL handshake.
 *
 * Adds client to tracking, sets up cancellation slot, performs SSL handshake if enabled,
 * then delegates to process_session.
 *
 * @param socket Accepted TCP socket.
 * @param client_id Unique client ID.
 * @return asio::awaitable<void> Coroutine for client lifecycle.
 */
asio::awaitable<void> Server::handle_client(tcp::socket socket, size_t client_id) {
    add_client(client_id);
    asio::cancellation_slot token = get_client_slot(client_id);

    struct ScopeErase {
        Server *self;
        size_t id;
        ~ScopeErase() { self->remove_client(id); }
    } guard{this, client_id};

    if (use_ssl_) {
#ifdef USE_SSL
        asio::ssl::stream<tcp::socket> ssl_stream(std::move(socket), ssl_context_);
        co_await ssl_stream.async_handshake(asio::ssl::stream_base::server,
                                            asio::bind_cancellation_slot(token, asio::use_awaitable));
        // std::cout << "Client " << client_id << " use SSL\n";
        co_await process_session(ssl_stream, token, client_id);
#else
        std::cerr << "SSL requested but OpenSSL disabled — using plain TCP.\n";
        co_await process_session(socket, token, client_id);
#endif
    } else {
        co_await process_session(socket, token, client_id);
    }
    co_return;
}

/**
 * @brief Reads and parses a full HTTP request (headers + body).
 *
 * Reads headers first, parses Request, then reads body if Content-Length present.
 *
 * @tparam Socket Socket type.
 * @param socket Socket to read from.
 * @param token Cancellation slot.
 * @return asio::awaitable<Request> Parsed Request object.
 */
template<typename Socket>
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

/**
 * @brief Writes an HTTP response, using fast-path for file sends if applicable.
 *
 * Dispatches to write_file_response if sendfile_path set, else write_regular_response.
 *
 * @tparam Socket Socket type.
 * @param socket Socket to write to.
 * @param response Response to serialize and send.
 * @param token Cancellation slot.
 */
template<typename Socket>
asio::awaitable<void> Server::do_write(
    Socket &socket,
    Response &response,
    asio::cancellation_slot token
) {
    if (!response.sendfile_path.empty()) {
        co_await write_file_response(socket, response, token);
    } else {
        co_await write_regular_response(socket, response, token);
    }
}

/**
 * @brief Writes a regular (non-file) HTTP response: headers + body.
 *
 * Prepares headers with Content-Length, writes headers then body (or just headers if empty).
 *
 * @tparam Socket Socket type.
 * @param socket Socket to write to.
 * @param response Response object.
 * @param token Cancellation slot.
 */
template<typename Socket>
asio::awaitable<void> Server::write_regular_response(
    Socket &socket,
    Response &response,
    asio::cancellation_slot token
) {
    std::uintmax_t body_size = response.body.size();
    std::string head = prepare_headers(response, body_size);

    if (body_size == 0) {
        co_await asio::async_write(
            socket,
            asio::buffer(head),
            asio::bind_cancellation_slot(token, asio::use_awaitable)
        );
        co_return;
    }

    std::array<asio::const_buffer, 2> bufs = {
        asio::buffer(head),
        asio::buffer(response.body)
    };

    co_await asio::async_write(
        socket,
        bufs,
        asio::bind_cancellation_slot(token, asio::use_awaitable)
    );
}

/**
 * @brief Windows-specific fast-path using TransmitFile for zero-copy file sending.
 *
 * Sends file headers first, then entire file via TransmitFile API for efficiency.
 *
 * @tparam Socket Must be tcp::socket (not SSL).
 * @param socket Native socket handle accessible.
 * @param path Full file path.
 * @param size File size.
 * @return asio::awaitable<bool> True if fully sent, false on failure.
 */
#ifdef _WIN32
template<typename Socket>
asio::awaitable<bool> Server::transmitfile_fast_path(
    Socket &socket,
    const std::string &path,
    std::uintmax_t size
) {
    // Open the file for reading
    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        co_return false;
    }

    (void) size;

    SOCKET s = socket.native_handle();

    BOOL ok = TransmitFile(
        s,
        hFile,
        0, // 0 -> send entire file
        0, // system default send size
        nullptr,
        nullptr,
        0 // no special flags for now
    );

    CloseHandle(hFile);

    if (!ok) {
        co_return false;
    }

    co_return true;
}
#endif

/**
 * @brief Linux-specific fast-path using sendfile for zero-copy file sending.
 *
 * Sends headers first, then file in chunks via sendfile(2), handling EAGAIN with wait.
 *
 * @tparam Socket Socket type.
 * @param socket Socket.
 * @param native_sock Native file descriptor.
 * @param path File path.
 * @param size File size.
 * @return asio::awaitable<bool> True if fully sent.
 */
#ifdef __linux__
template<typename Socket>
asio::awaitable<bool> Server::sendfile_fast_path(
    Socket &socket,
    int &native_sock,
    const std::string &path,
    std::uintmax_t size
) {
    int file_fd = open(path.c_str(), O_RDONLY);
    if (file_fd < 0) {
        co_return false;
    }

    off_t offset = 0;
    std::size_t remaining = size;

    while (remaining > 0) {
        ssize_t sent = sendfile(native_sock, file_fd, &offset, remaining);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                co_await socket.lowest_layer().async_wait(
                    asio::socket_base::wait_write,
                    asio::use_awaitable
                );
                continue;
            }
            break;
        }
        if (sent == 0) {
            break;
        }
        remaining -= static_cast<std::size_t>(sent);
    }

    close(file_fd);
    co_return remaining == 0;
}
#endif

/**
 * @brief Writes a file-based response, preferring platform zero-copy if available.
 *
 * Tries sendfile/TransmitFile fast-paths first (platform-specific), falls back to
 * buffered stream_file read/write loop with 64KB chunks.
 *
 * @tparam Socket Socket type.
 * @param socket Socket.
 * @param response Response with sendfile_path/size set.
 * @param token Cancellation slot.
 */
template<typename Socket>
asio::awaitable<void> Server::write_file_response(
    Socket &socket,
    Response &response,
    asio::cancellation_slot token
) {
    const std::string &path = response.sendfile_path.string();

#ifdef __linux__
    int native_sock = get_native_handle(socket);
    if (native_sock != -1) {
        if (response.sendfile_size > 0) {
            std::string head = prepare_headers(response, response.sendfile_size);

            co_await asio::async_write(
                socket,
                asio::buffer(head),
                asio::bind_cancellation_slot(token, asio::use_awaitable)
            );

            bool ok = co_await sendfile_fast_path(socket, native_sock, path, response.sendfile_size);
            if (ok) {
                co_return;
            }

            co_return;
        }
    }
#endif

#ifdef _WIN32
    // Only use TransmitFile for plain TCP sockets, not SSL.
    if constexpr (std::is_same_v<Socket, asio::ip::tcp::socket>) {
        if (response.sendfile_size > 0) {
            std::string head = prepare_headers(response, response.sendfile_size);

            co_await asio::async_write(
                socket,
                asio::buffer(head),
                asio::bind_cancellation_slot(token, asio::use_awaitable)
            );

            bool ok = co_await transmitfile_fast_path(socket, path, response.sendfile_size);
            if (ok) {
                co_return;
            }
        }
    }
#endif

    asio::stream_file file(co_await asio::this_coro::executor);
    std::error_code ec;
    file.open(path.c_str(), asio::file_base::read_only, ec);
    if (ec) co_return;

    auto file_sz = file.size(ec);
    if (ec || file_sz == static_cast<std::uintmax_t>(-1)) co_return;

    std::string head = prepare_headers(response, file_sz);

    co_await asio::async_write(
        socket,
        asio::buffer(head),
        asio::bind_cancellation_slot(token, asio::use_awaitable)
    );

    std::array<char, 64 * 1024> buf{};
    std::uintmax_t remaining = file_sz;

    while (remaining > 0) {
        std::size_t to_read =
                std::min<std::uintmax_t>(remaining, buf.size());

        std::size_t n = co_await file.async_read_some(
            asio::buffer(buf.data(), to_read),
            asio::bind_cancellation_slot(token, asio::use_awaitable)
        );
        if (n == 0) break;

        remaining -= n;

        co_await asio::async_write(
            socket,
            asio::buffer(buf.data(), n),
            asio::bind_cancellation_slot(token, asio::use_awaitable)
        );
    }
}

/**
 * @brief Adds a client to the cancellation tracking map.
 *
 * Thread-safe insertion into client_cancel_ map.
 *
 * @param client_id Unique client ID.
 */
void Server::add_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    client_cancel_.try_emplace(client_id);
}

/**
 * @brief Retrieves the cancellation slot for a specific client.
 *
 * Thread-safe lookup; returns default-constructed slot if not found.
 *
 * @param client_id Client ID.
 * @return asio::cancellation_slot For the client.
 */
asio::cancellation_slot Server::get_client_slot(size_t client_id) {
    std::lock_guard lock(mutex_);
    auto it = client_cancel_.find(client_id);
    return (it != client_cancel_.end()) ? it->second.slot() : asio::cancellation_slot();
}

/**
 * @brief Emits cancellation signal for a specific client.
 *
 * Thread-safe: copies signal pointer under lock, emits outside.
 *
 * @param client_id Client ID to cancel.
 */
void Server::emit_client(size_t client_id) {
    asio::cancellation_signal *sig = nullptr;
    {
        std::lock_guard lk(mutex_);
        auto it = client_cancel_.find(client_id);
        if (it != client_cancel_.end()) sig = &it->second;
    }
    if (sig) sig->emit(asio::cancellation_type::all);
}

/**
 * @brief Emits cancellation to all tracked clients.
 *
 * Collects IDs under lock, emits serially to avoid lock contention.
 */
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

/**
 * @brief Removes a client from tracking.
 *
 * Thread-safe erase.
 *
 * @param client_id Client ID.
 */
void Server::remove_client(size_t client_id) {
    std::lock_guard lock(mutex_);
    client_cancel_.erase(client_id);
}

/**
 * @brief Sets up SSL context with certificates.
 *
 * Loads chain and private key from "certs/localhost.crt/key". Returns false on failure.
 * Only compiled if USE_SSL defined.
 *
 * @return bool True if setup successful.
 */
#ifdef USE_SSL
bool Server::setup_ssl() {
    if (use_ssl_) {
        ssl_context_.set_options(
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2);

        try {
            ssl_context_.use_certificate_chain_file("certs/localhost.crt");
            ssl_context_.use_private_key_file("certs/localhost.key", asio::ssl::context::pem);
        } catch (const asio::system_error &e) {
            std::cerr << "SSL setup failed: " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}
#else
bool Server::setup_ssl() {
    return false;
}
#endif

/**
 * @brief Maps file extensions to MIME types.
 *
 * Static lookup table for common types, defaults to application/octet-stream.
 * Case-insensitive.
 *
 * @param ext File extension (with or without dot).
 * @return std::string MIME type string.
 */
static std::string ext_type(const std::string &ext) {
    std::string e = ext;
    if (!e.empty() && e.front() == '.')
        e.erase(0, 1);

    for (auto &ch: e) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    static const std::unordered_map<std::string, std::string> k = {
        {"html", "text/html; charset=utf-8"},
        {"htm", "text/html; charset=utf-8"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"mjs", "application/javascript"},
        {"json", "application/json"},
        {"txt", "text/plain; charset=utf-8"},
        {"xml", "application/xml"},
        {"svg", "image/svg+xml"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"wasm", "application/wasm"},
        {"mp4", "video/mp4"}
    };

    auto it = k.find(e);
    return (it != k.end()) ? it->second : std::string("application/octet-stream");
}

/**
 * @brief Serves static files from mounted directories.
 *
 * Matches longest prefix mount, resolves path safely (no directory traversal),
 * sets MIME type, configures sendfile for zero-copy.
 *
 * @param req GET request.
 * @return asio::awaitable<Response> 404 if not found/mount mismatch, else file response.
 */
asio::awaitable<Response> Server::serve_static(const Request &req) {
    if (req.method != Method::GET)
        co_return Response::bad_request("Method not supported");

    std::string url_prefix;
    fs::path root;

    for (auto &[prefix, rt]: static_mounts_) {
        if (req.path.starts_with(prefix)) {
            url_prefix = prefix;
            root = rt;
            break;
        }
    }
    if (url_prefix.empty())
        co_return Response::not_found("Mount does not exist");

    std::string tail = req.path.substr(url_prefix.size());
    if (tail.empty() || tail.back() == '/')
        tail = tail + "index.html";

    fs::path srv_path = root / fs::path(tail).relative_path();
    std::error_code ec;

    fs::path canon_srv_path = weakly_canonical(srv_path, ec);
    if (ec || canon_srv_path.native().compare(0, root.native().size(), root.native()) != 0)
        co_return Response::not_found("Different root path");

    Response res = Response::text("OK");

    if (!fs::exists(canon_srv_path, ec) || ec)
        co_return Response::not_found("File not found in root directory");

    res.set_header("content-type", ext_type(canon_srv_path.extension().string()));

    res.sendfile_path = canon_srv_path;
    res.sendfile_size = fs::file_size(canon_srv_path);
    co_return res;
}

/**
 * @brief Mounts a filesystem directory for static serving at a URL prefix.
 *
 * Canonicalizes root path if relative. Enables static file serving for matching prefixes.
 *
 * @param url_prefix URL prefix (e.g., "/static").
 * @param root Filesystem path to serve.
 */
void Server::MountStatic(std::string url_prefix, fs::path root) {
    if (!root.empty() && root.is_relative())
        root = fs::weakly_canonical(root);
    static_mounts_.emplace(std::move(url_prefix), std::move(root));
}

/**
 * @brief Posts a task to the io_context for asynchronous execution.
 *
 * Useful for non-network tasks from other threads.
 *
 * @param task std::function<void()> to execute.
 */
void Server::post_task(std::function<void()> task) {
    asio::post(io_context_, std::move(task));
}

/**
 * @brief Closes all active WebSockets during shutdown.
 *
 * Collects shared_ptrs under lock, sends close frames with code 1001.
 */
void Server::close_websockets() {
    std::vector<std::shared_ptr<WebSocket> > to_close;
    {
        std::lock_guard lock(ws_mutex);
        to_close.reserve(websockets_.size());
        for (auto &entry: websockets_) {
            if (entry.second) {
                to_close.push_back(entry.second);
            }
        }
    }
    for (auto &ws: to_close) {
        if (!ws) continue;
        ws->close(1001, "Server stopped working");
    }
}