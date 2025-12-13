/**
 * @file http_response.h
 * @brief Defines the Response struct for HTTP responses.
 */

#ifndef WEBSOCKETLIB_HTTP_RESPONSE_H
#define WEBSOCKETLIB_HTTP_RESPONSE_H

#include "request.h"
#include "status_codes.h"
#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief Represents an HTTP response.
 *
 * Encapsulates the HTTP version, status code, headers, body, and optional
 * file information for sendfile responses. Provides utility methods to
 * construct common response types, set headers, and serialize the response.
 */
struct Response {
    std::string version = "HTTP/1.1"; /**< HTTP version string (e.g., "HTTP/1.1") */
    int status = OK_200;              /**< HTTP status code */
    Headers headers;                  /**< Map of HTTP headers */
    std::string body;                 /**< Response body */

    fs::path sendfile_path{};         /**< Optional path to a file to serve */
    size_t sendfile_size = 0;         /**< Size of the file to send */

    /** @brief Checks if a header exists in the response. */
    bool has_header(const std::string &key) const;

    /** @brief Gets the value of a header. Returns empty string if not found. */
    std::string get_header_value(const std::string &key) const;

    /** @brief Sets the response body and Content-Type header. */
    void set_content(const std::string &s, const std::string &content_type);

    /** @brief Sets a redirect response with a given URL and status code. */
    void set_redirect(const std::string &url, int st = Found_302);

    /** @brief Sets or overwrites a response header. */
    void set_header(const std::string &key, const std::string &value);

    /** @brief Returns the Content-Length of the response body. */
    std::optional<size_t> content_length() const;

    /** @brief Returns the Content-Type header value. */
    std::string content_type() const;

    /** @brief Creates a text/plain response with optional charset and status code. */
    static Response text(const std::string& s, int st = OK_200, const std::string &charset = "utf-8");

    /** @brief Creates an application/json response with optional status code. */
    static Response json(const std::string& s, int st = OK_200);

    /** @brief Creates an HTML response with optional status code. */
    static Response html(const std::string& s, int st = OK_200);

    /** @brief Creates a 404 Not Found response with optional message. */
    static Response not_found(const std::string &what = "Not Found");

    /** @brief Creates a 400 Bad Request response with optional message. */
    static Response bad_request(const std::string &message = "Bad Request");

    /** @brief Returns the standard reason phrase for a given status code. */
    static std::string_view reason_phrase(int st) noexcept;

    /** @brief Serializes the full response (headers + body) to a raw HTTP string. */
    std::string to_string() const;

    /** @brief Serializes only the response headers to a string. */
    std::string to_string_header() const;
};

#endif // WEBSOCKETLIB_HTTP_RESPONSE_H
