#include "http/request.h"
#include <sstream>

/**
 * @file request.cpp
 * @brief Implementation of HTTP Request helper functions.
 *
 * This file defines methods for the `Request` class, which represents an HTTP request.
 * Functions include header and query parameter access, content-type checks, serialization
 * to string, HTTP method conversions, and WebSocket upgrade detection.
 *
 * Example usage:
 * @code
 * Request req;
 * if (req.has_header("Content-Type")) {
 *     auto ct = req.content_type();
 * }
 * std::string raw = req.to_string();
 * @endcode
 */

/**
 * @brief Check if the request contains a specific header.
 * @param key Header name (case-insensitive).
 * @return true if the header exists, false otherwise.
 */
bool Request::has_header(const std::string &key) const {
    return headers.contains(key);
}

/**
 * @brief Get the value of a specific header.
 * @param key Header name (case-insensitive).
 * @return Value of the header, or empty string if not present.
 */
std::string Request::get_header_value(const std::string &key) const {
    auto it = headers.find(key);
    return (it == headers.end()) ? std::string{} : it->second;
}

/**
 * @brief Check if a query parameter exists.
 * @param key Parameter name.
 * @return true if the parameter exists, false otherwise.
 */
bool Request::has_param(const std::string &key) const {
    return query_params.contains(key);
}

/**
 * @brief Get the value of a query parameter.
 * @param key Parameter name.
 * @param id Index in case multiple values exist for the same key.
 * @return Parameter value, or empty string if not found.
 */
std::string Request::get_param_value(const std::string &key, size_t id) const {
    auto [it, end] = query_params.equal_range(key);
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

/**
 * @brief Count the number of values for a query parameter.
 * @param key Parameter name.
 * @return Number of values associated with the key.
 */
size_t Request::get_param_value_count(const std::string &key) const {
    auto [it, end] = query_params.equal_range(key);
    return std::distance(it, end);
}

/**
 * @brief Get the Content-Length of the request body.
 * @return Number of bytes if Content-Length is valid, std::nullopt otherwise.
 */
std::optional<size_t> Request::content_length() const {
    auto s = get_header_value("content-length");
    if (s.empty()) return std::nullopt;
    try {
        size_t index = 0;
        auto val = std::stoull(s, &index);
        if (index != s.size()) return std::nullopt;
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Get the Content-Type of the request.
 * @return MIME type string, or empty if not present.
 */
std::string Request::content_type() const {
    return get_header_value("content-type");
}

/**
 * @brief Convert a Method enum to its string representation.
 * @param m HTTP method enum.
 * @return Method as string, or "UNKNOWN" if invalid.
 */
std::string Request::method_str(Method m) noexcept {
    static const std::string names[] = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
    };
    const auto index = static_cast<size_t>(m);
    return index < std::size(names) ? names[index] : "UNKNOWN";
}

/**
 * @brief Convert a string to HTTP Method enum.
 * @param s Method name string.
 * @return Corresponding Method enum, or Method::UNKNOWN if invalid.
 */
Method Request::method_enum(const std::string &s) noexcept {
    static const std::unordered_map<std::string, Method> names = {
        {"GET", Method::GET},
        {"POST", Method::POST},
        {"PUT", Method::PUT},
        {"DELETE", Method::DELETE_},
        {"PATCH", Method::PATCH},
        {"HEAD", Method::HEAD},
        {"OPTIONS", Method::OPTIONS}
    };

    auto it = names.find(s);
    return (it == names.end()) ? Method::UNKNOWN : it->second;
}

/**
 * @brief Check if the request Content-Type matches a given subtype.
 * @param sub_type Expected MIME type (without parameters).
 * @return true if Content-Type matches, false otherwise.
 */
bool Request::has_content_type(const std::string &sub_type) const {
    auto ct = content_type();
    auto pos = ct.find(';');

    if (pos != std::string::npos)
        ct.resize(pos);

    return ct == sub_type;
}

/**
 * @brief Serialize the request to a raw HTTP string.
 * @return Complete HTTP request as string (first line + headers + optional body).
 */
std::string Request::to_string() const {
    std::ostringstream ss;

    ss << method_str(method) << " " << target << " " << version << "\r\n";

    for (const auto &h: headers) {
        ss << h.first << ": " << h.second << "\r\n";
    }

    if (!body.empty() && !headers.contains("content-length")) {
        ss << "Content-Length: " << body.size() << "\r\n";
    }

    ss << "\r\n";

    ss << body;

    return ss.str();
}

/**
 * @brief Check if the request is a WebSocket upgrade request.
 * @return true if all required WebSocket headers are present, false otherwise.
 */
bool Request::is_ws_upgrade() const {
    if (!headers.contains("Upgrade") || !headers.contains("Connection") || !headers.contains("Sec-WebSocket-Key")) {
        return false;
    }
    return true;
}
