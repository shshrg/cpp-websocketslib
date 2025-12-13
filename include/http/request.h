/**
 * @file http_request.h
 * @brief Defines structures and utilities for HTTP requests.
 */

#ifndef WEBSOCKETLIB_HTTP_REQUEST_H
#define WEBSOCKETLIB_HTTP_REQUEST_H

#include <chrono>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>

/**
 * @brief Enum representing HTTP request methods.
 */
enum class Method : uint8_t {
    GET,        /**< HTTP GET method */
    POST,       /**< HTTP POST method */
    PUT,        /**< HTTP PUT method */
    DELETE_,    /**< HTTP DELETE method (DELETE_ to avoid keyword conflict) */
    PATCH,      /**< HTTP PATCH method */
    HEAD,       /**< HTTP HEAD method */
    OPTIONS,    /**< HTTP OPTIONS method */
    UNKNOWN     /**< Unknown or unsupported HTTP method */
};

/**
 * @brief Case-insensitive hash functor for string keys.
 */
struct CaseInsensitiveHash {
    /**
     * @brief Compute a case-insensitive hash of a string.
     * @param s Input string.
     * @return Hash value.
     */
    size_t operator()(const std::string &s) const noexcept;
};

/**
 * @brief Case-insensitive equality functor for string keys.
 */
struct CaseInsensitiveEqual {
    using is_transparent = void;

    /**
     * @brief Compare two strings for equality ignoring case.
     * @param s1 First string.
     * @param s2 Second string.
     * @return true if strings are equal ignoring case, false otherwise.
     */
    bool operator()(const std::string & s1, const std::string & s2) const noexcept;
};

/**
 * @brief Type alias for HTTP headers map with case-insensitive keys.
 */
using Headers = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;

/**
 * @brief Type alias for HTTP query parameters (multimap to allow duplicate keys).
 */
using Params = std::multimap<std::string, std::string>;

/**
 * @brief Represents a parsed HTTP request.
 *
 * Contains HTTP method, path, version, full target, headers, query parameters, and body.
 */
struct Request {
    Method method = Method::UNKNOWN;   /**< HTTP method */
    std::string path;                  /**< Path part of the URL */
    std::string version;               /**< HTTP version (e.g., "HTTP/1.1") */
    std::string target;                /**< Full target including query string */

    Headers headers;                   /**< HTTP headers */
    Params query_params;               /**< Parsed query parameters */
    std::string body;                  /**< Request body */

    /** @brief Check if a header exists. */
    bool has_header(const std::string & key) const;

    /** @brief Get the value of a header. Returns empty string if not found. */
    std::string get_header_value(const std::string & key) const;

    /** @brief Check if a query parameter exists. */
    bool has_param(const std::string & key) const;

    /** @brief Get the value of a query parameter by key and index. */
    std::string get_param_value(const std::string & key, size_t id = 0) const;

    /** @brief Get the number of values for a query parameter. */
    size_t get_param_value_count(const std::string & key) const;

    /** @brief Convert the request to a raw HTTP string. */
    std::string to_string() const;

    /** @brief Get Content-Length header value if present. */
    std::optional<size_t> content_length() const;

    /** @brief Get Content-Type header value. */
    std::string content_type() const;

    /** @brief Convert Method enum to string. */
    static std::string method_str(Method m) noexcept;

    /** @brief Convert string to Method enum. */
    static Method method_enum(const std::string & s) noexcept;

    /** @brief Check if the Content-Type header contains a specific subtype. */
    bool has_content_type(const std::string & sub_type) const;

    /** @brief Check if the request is a WebSocket upgrade request. */
    bool is_ws_upgrade() const;
};

#endif // WEBSOCKETLIB_HTTP_REQUEST_H
