/**
 * @file http_client_example.cpp
 * @brief Simple synchronous HTTP client implementation using ASIO.
 *
 * Provides a lightweight HTTP client capable of performing GET and POST
 * requests over plain TCP or SSL (if compiled with USE_SSL).
 * It manages connection, request formation, sending, and response reading.
 * The client stops automatically on connection errors.
 */

#include "http_client_example.h"
#include <iostream>

/**
 * @brief Constructor.
 * @param use_ssl Enable SSL/TLS if true.
 */
HttpClient::HttpClient(bool use_ssl)
    : use_ssl_(use_ssl)
    , stopped(false)
#ifdef USE_SSL
    , ssl_ctx_(asio::ssl::context::tls_client)
    , ssl_socket_(internal_io_, ssl_ctx_)
#endif
{ }

#ifdef USE_SSL
/**
 * @brief Set certificate verification file for SSL connections.
 * @param file Path to the PEM certificate file.
 */
void HttpClient::set_verify_cert_file(const std::string& file) {
    if (!use_ssl_) return;
    ssl_ctx_.load_verify_file(file);
    ssl_ctx_.set_verify_mode(asio::ssl::verify_peer);
}
#endif

/**
 * @brief Connect to the server using TCP or SSL.
 * @param host Server hostname or IP.
 * @param port Server port as string.
 *
 * Resolves endpoints, establishes a connection, and performs SSL handshake if enabled.
 */
void HttpClient::connect(const std::string& host, const std::string& port) {
    host_ = host;
    asio::ip::tcp::resolver resolver(internal_io_);
    auto endpoints = resolver.resolve(host, port);

    if (use_ssl_) {
#ifdef USE_SSL
        asio::connect(ssl_socket_.lowest_layer(), endpoints);
        SSL_set_tlsext_host_name(ssl_socket_.native_handle(), host.c_str());
        ssl_socket_.handshake(asio::ssl::stream_base::client);
#else
        throw std::runtime_error("SSL not supported");
#endif
    } else {
        asio::connect(socket_, endpoints);
    }
}

/**
 * @brief Perform a GET request.
 * @param target Request path.
 * @param headers Optional additional headers.
 * @return Response body as string.
 */
std::string HttpClient::get(const std::string& target,
                            const std::map<std::string, std::string>& headers) {
    if (stopped) return {};
    return send_request(Method::GET, target, "", headers);
}

/**
 * @brief Perform a POST request.
 * @param target Request path.
 * @param body Request body.
 * @param headers Optional additional headers.
 * @return Response body as string.
 */
std::string HttpClient::post(const std::string& target,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers) {
    if (stopped) return {};
    return send_request(Method::POST, target, body, headers);
}

/**
 * @brief Send a request over TCP/SSL and read the response.
 * @param method HTTP method.
 * @param target Request path.
 * @param body Optional request body.
 * @param headers Optional headers.
 * @return Full HTTP response as string.
 */
std::string HttpClient::send_request(Method method, const std::string& target,
                                     const std::string& body,
                                     const std::map<std::string, std::string>& headers) {
    // Prepare request object
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

    // Send request via SSL or plain socket
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

/**
 * @brief Read the HTTP response from the socket.
 * @return Complete HTTP response as string.
 *
 * Reads headers first (up to \r\n\r\n), then reads body until EOF or stream truncation.
 * Handles SSL and plain TCP sockets.
 */
std::string HttpClient::read_response() {
    asio::streambuf buf;
    std::string response;
    std::error_code ec;

    // Read headers
    if (use_ssl_) {
#ifdef USE_SSL
        asio::read_until(ssl_socket_, buf, "\r\n\r\n", ec);
#else
        throw std::runtime_error("SSL not supported");
#endif
    } else {
        asio::read_until(socket_, buf, "\r\n\r\n", ec);
    }

    if (ec && ec != asio::error::eof
#ifdef USE_SSL
        && ec != asio::ssl::error::stream_truncated
#endif
    ) {
        std::cerr << "[CLIENT] Error header read: " << ec.message() << std::endl;
        stopped = true;
        return "";
    }

    // Extract headers into string
    std::istream is(&buf);
    std::string line;
    std::string headers;
    while (std::getline(is, line) && line != "\r") {
        headers += line + "\n";
    }
    response = headers;

    // Append any leftover bytes in buffer
    std::string leftover(std::istreambuf_iterator<char>(is), {});
    response += leftover;

    // Read the rest of the body
    std::array<char, 8192> tmp{};
    for (;;) {
        std::error_code ec_body;
        std::size_t n = 0;

        if (use_ssl_) {
#ifdef USE_SSL
            n = ssl_socket_.read_some(asio::buffer(tmp), ec_body);
#else
            throw std::runtime_error("SSL not supported");
#endif
        } else {
            n = socket_.read_some(asio::buffer(tmp), ec_body);
        }

        if (n > 0)
            response.append(tmp.data(), n);

        if (ec_body == asio::error::eof
#ifdef USE_SSL
            || (use_ssl_ && ec_body == asio::ssl::error::stream_truncated)
#endif
        ) {
            break;
        }

        if (ec_body) {
            std::cerr << "[CLIENT] Error body read: " << ec_body.message() << std::endl;
            stopped = true;
            break;
        }
    }

    stopped = true;
    return response;
}
