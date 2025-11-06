#include "server.h"
#include <iostream>
#include <ranges>
#include <thread>
#include "http/utils.h"
#include "io_helpers.h"
#include <asio/stream_file.hpp>

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

        asio::co_spawn(io_context_,
                       handle_client(std::move(socket), client_id),
                       asio::detached);
    }
}


template<typename Socket>
asio::awaitable<void> Server::process_session(Socket &socket, asio::cancellation_slot token) {
    Request req = co_await do_read(socket, token);
    std::cout << req.to_string() << "\n";

    if (req.is_ws_upgrade()) {
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
        co_await process_session_ws(socket, req.path, key, token);
        co_return;
    }
    Response resp = co_await handle_request(req);
    co_await do_write(socket, resp, token);
}

template<typename Socket>
asio::awaitable<void> Server::process_session_ws(Socket &socket,
                                         const std::string &path,
                                         const std::string &sec_ws_key,
                                         asio::cancellation_slot token)
{
    std::string accept = ws_accept_key(sec_ws_key);
    Response resp = build_101_response(accept);
    co_await asio::async_write(socket, asio::buffer(resp.to_string()),
                               asio::bind_cancellation_slot(token, asio::use_awaitable));

    const WsHandlers *handlers = find_ws(path);
    if (!handlers) co_return;

    if (handlers->on_open) handlers->on_open();

    co_await do_read_loop(socket, handlers, token);

    co_return;
}



asio::awaitable<Response> Server::handle_request(const Request &req) {
    auto method_it = routes_.find(req.method);
    if (method_it == routes_.end()) co_return Response::not_found();

    auto &table = method_it->second;

    if (auto it = table.find(req.path); it != table.end()) {
        co_return co_await it->second(req);
    }

    if (!static_mounts_.empty()) {
        Response res = co_await serve_static(req);
        if (res.status != NotFound_404)
            co_return res;
    }
    co_return Response::not_found();
}

asio::awaitable<void> Server::handle_client(tcp::socket socket, size_t client_id) {
    add_client(client_id);
    asio::cancellation_slot token = get_client_slot(client_id);

    struct ScopeErase {
        Server *self;
        size_t id;
        ~ScopeErase() { self->remove_client(id); }
    } guard{this, client_id};

    if (use_ssl_) {
        std::cout << "Client " << client_id << " use TLS\n";
        asio::ssl::stream<tcp::socket> ssl_stream(std::move(socket), ssl_context_);
        co_await ssl_stream.async_handshake(asio::ssl::stream_base::server,
                                            asio::bind_cancellation_slot(token, asio::use_awaitable));
        co_await process_session(ssl_stream, token);
    } else {
        co_await process_session(socket, token);
    }
    remove_client(client_id);
    co_return;
}

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

template<typename Socket>
asio::awaitable<void> Server::do_write(Socket &socket, const Response &response_in, asio::cancellation_slot token) {
    Response response = response_in;

    if (!response.sendfile_path.empty()) {
        asio::stream_file file(co_await asio::this_coro::executor);
        std::error_code ec;
        file.open(response.sendfile_path.string().c_str(), asio::file_base::read_only, ec);
        if (ec) co_return;

        auto file_sz = file.size(ec);
        if (!ec && file_sz > 0) {
            response.body.resize(file_sz);
            size_t off = 0;
            while (off < response.body.size()) {
                size_t read_bytes = co_await file.async_read_some(
                    asio::buffer(response.body.data() + off, response.body.size() - off),
                    asio::bind_cancellation_slot(token, asio::use_awaitable));

                if (read_bytes == 0) break;
                off += read_bytes;
            }
            response.body.resize(off);
        } else {
            std::array<char, 64 * 1024> buffer{};
            while (true) {
                std::size_t read_bytes = co_await file.async_read_some(
                    asio::buffer(buffer),
                    asio::bind_cancellation_slot(token, asio::use_awaitable));

                if (read_bytes == 0) break;

                response.body.append(buffer.data(), read_bytes);
            }
        }

        response.set_header("Content-Length", std::to_string(response.body.size()));
    }
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
    asio::cancellation_signal *sig = nullptr;
    {
        std::lock_guard lk(mutex_);
        auto it = client_cancel_.find(client_id);
        if (it != client_cancel_.end()) sig = &it->second;
    }
    if (sig) sig->emit(asio::cancellation_type::all);
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

    fs::path srv_path = root / fs::path(tail);
    std::error_code ec;

    fs::path canon_srv_path = weakly_canonical(srv_path, ec);
    if (ec || canon_srv_path.native().compare(0, root.native().size(), root.native()) != 0)
        co_return Response::not_found("Different root path");

    Response res = Response::text("OK");

    if (!fs::exists(canon_srv_path, ec) || ec)
        co_return Response::not_found("File not found in root directory");

    res.set_header("content-type", ext_type(canon_srv_path.extension()));

    res.sendfile_path = canon_srv_path;
    res.sendfile_size = fs::file_size(canon_srv_path);
    co_return res;
}

void Server::MountStatic(std::string url_prefix, fs::path root) {
    if (!root.empty() && root.is_relative())
        root = fs::weakly_canonical(root);
    static_mounts_.emplace(std::move(url_prefix), std::move(root));
}

void Server::post_task(std::function<void()> task) {
    asio::post(io_context_, std::move(task));
}
