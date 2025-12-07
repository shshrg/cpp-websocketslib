#ifndef WEBSOCKETLIB_HTTP_CLIENT_H
#define WEBSOCKETLIB_HTTP_CLIENT_H

#include <asio.hpp>
#include <string>
#include <map>
#include <array>
#include <iostream>
#include <atomic>
#include "http/request.h"

#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif

class HttpClient
{
public:
    explicit HttpClient(bool use_ssl = false);

#ifdef USE_SSL
    void set_verify_cert_file(const std::string& file);
#endif

    void connect(const std::string& host, const std::string& port);

    std::string get(const std::string& target,
                    const std::map<std::string, std::string>& headers = {});

    std::string post(const std::string& target,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {});

    std::atomic<bool> stopped;

private:
    asio::io_context internal_io_;
    bool use_ssl_;
    std::string host_;

#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_;
#endif

    asio::ip::tcp::socket socket_{internal_io_};

    std::string send_request(Method method, const std::string& target,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers);

    std::string read_response();
};
#endif //WEBSOCKETLIB_HTTP_CLIENT_H