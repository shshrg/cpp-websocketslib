/**
 * @file http_client.h
 * @brief Simple synchronous HTTP client using ASIO, optionally supporting SSL.
 */

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

/**
 * @brief Synchronous HTTP client supporting GET and POST requests.
 *
 * Can optionally use SSL/TLS for HTTPS connections.
 */
class HttpClient
{
public:
    /**
     * @brief Construct an HttpClient instance.
     * @param use_ssl If true, enables SSL/TLS support.
     */
    explicit HttpClient(bool use_ssl = false);

#ifdef USE_SSL
    /**
     * @brief Set the certificate verification file for SSL connections.
     * @param file Path to PEM certificate file.
     */
    void set_verify_cert_file(const std::string& file);
#endif

    /**
     * @brief Connect to a server at the given host and port.
     * @param host Server hostname or IP.
     * @param port Server port as string (e.g., "80" or "443").
     */
    void connect(const std::string& host, const std::string& port);

    /**
     * @brief Perform an HTTP GET request.
     * @param target Request target (e.g., "/api/data").
     * @param headers Optional additional headers to include.
     * @return Response body as a string.
     */
    std::string get(const std::string& target,
                    const std::map<std::string, std::string>& headers = {});

    /**
     * @brief Perform an HTTP POST request.
     * @param target Request target (e.g., "/api/data").
     * @param body Request body content.
     * @param headers Optional additional headers to include.
     * @return Response body as a string.
     */
    std::string post(const std::string& target,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {});

    std::atomic<bool> stopped; /**< Flag indicating if client is stopped. */

private:
    asio::io_context internal_io_;  /**< Internal IO context for synchronous operations */
    bool use_ssl_;                  /**< True if SSL/TLS is enabled */
    std::string host_;              /**< Hostname or IP of the server */

#ifdef USE_SSL
    asio::ssl::context ssl_ctx_;   /**< SSL context for secure connections */
    asio::ssl::stream<asio::ip::tcp::socket> ssl_socket_; /**< SSL/TLS socket */
#endif

    asio::ip::tcp::socket socket_{internal_io_}; /**< TCP socket for non-SSL connections */

    /**
     * @brief Build and send an HTTP request, then return the response body.
     * @param method HTTP method.
     * @param target Request target (path).
     * @param body Request body (for POST/PUT).
     * @param headers Optional headers.
     * @return Response body as string.
     */
    std::string send_request(Method method, const std::string& target,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers);

    /**
     * @brief Read a full HTTP response from the connected server.
     * @return Response body as string.
     */
    std::string read_response();
};

#endif // WEBSOCKETLIB_HTTP_CLIENT_H
