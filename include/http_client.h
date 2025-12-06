#ifndef HTTP_CLIENT_HTTP_CLIENT_H
#define HTTP_CLIENT_HTTP_CLIENT_H

#include <asio.hpp>
#include <string>
#include <map>
#include <array>
#include <iostream>
#include "http/request.h"

#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif

class HttpClient {
public:
    explicit HttpClient(bool use_ssl = false)
        : use_ssl_(use_ssl)
#ifdef USE_SSL
        , ssl_ctx_(asio::ssl::context::tls_client)
        , ssl_socket_(internal_io_, ssl_ctx_)
#endif
    {}

    void connect(const std::string& host, const std::string& port) {
        host_ = host; // зберігаємо хост для заголовка Host
        asio::ip::tcp::resolver resolver(internal_io_);
        auto endpoints = resolver.resolve(host, port);

        if (use_ssl_) {
#ifdef USE_SSL
            asio::connect(ssl_socket_.lowest_layer(), endpoints);
            ssl_socket_.handshake(asio::ssl::stream_base::client);
#else
            throw std::runtime_error("SSL not supported, recompile with USE_SSL");
#endif
        } else {
            asio::connect(socket_, endpoints);
        }
    }

    std::string get(const std::string& target,
                    const std::map<std::string, std::string>& headers = {}) {
        return send_request(Method::GET, target, "", headers);
    }

    std::string post(const std::string& target,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) {
        return send_request(Method::POST, target, body, headers);
    }

private:
    asio::io_context internal_io_;
    bool use_ssl_;
    std::string host_; // зберігаємо для Host header

#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_;
#endif

    asio::ip::tcp::socket socket_{internal_io_};

    std::string send_request(Method method, const std::string& target,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers) {
        Request request;
        request.method = method;
        request.version = "HTTP/1.1";
        request.target = target;
        request.body = body;

        request.headers["Host"] = host_;
        request.headers["Connection"] = "close";

        if (method == Method::POST) {
            request.headers["Content-Length"] = std::to_string(body.size());
            request.headers["Content-Type"] = "text/plain; charset=utf-8";
        }

        for (const auto& kv : headers)
            request.headers[kv.first] = kv.second;

        std::string req_str = request.to_string();

        // пишемо в один і той же сокет
        if (use_ssl_) {
#ifdef USE_SSL
            asio::write(ssl_socket_, asio::buffer(req_str));
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::write(socket_, asio::buffer(req_str));
        }

        return read_response();
    }

    std::string read_response() {
        asio::streambuf buf;
        std::string response;

        if (use_ssl_) {
#ifdef USE_SSL
            asio::read_until(ssl_socket_, buf, "\r\n\r\n");
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            asio::read_until(socket_, buf, "\r\n\r\n");
        }

        std::istream is(&buf);
        std::string line;
        while (std::getline(is, line) && line != "\r") {
            response += line + "\n";
        }

        std::array<char, 8192> tmp{};
        for (;;) {
            std::error_code ec;
            std::size_t n = 0;
            if (use_ssl_) {
#ifdef USE_SSL
                n = ssl_socket_.read_some(asio::buffer(tmp), ec);
#else
                throw std::runtime_error("SSL not supported");
#endif
            } else {
                n = socket_.read_some(asio::buffer(tmp), ec);
            }
            if (n > 0)
                response.append(tmp.data(), n);

            if (ec == asio::error::eof) break;
            if (ec) throw std::runtime_error("Read error: " + ec.message());
        }

        return response;
    }
};

#endif // HTTP_CLIENT_HTTP_CLIENT_H
