#include "http/response.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * @file response.cpp
 * @brief Implementation of HTTP Response helper functions.
 *
 * This file defines methods for the `Response` class, which represents an HTTP response.
 * Functions include header access, content management, serialization, creation of common
 * response types (text, JSON, HTML), redirects, and standard status/reason handling.
 *
 * Example usage:
 * @code
 * Response res = Response::json("{\"success\":true}");
 * res.set_header("X-Custom", "value");
 * std::string raw_response = res.to_string();
 * @endcode
 */

/**
 * @brief Check if the response contains a specific header.
 * @param key Header name (case-insensitive).
 * @return true if header exists, false otherwise.
 */
bool Response::has_header(const std::string &key) const {
    return headers.contains(key);
}

/**
 * @brief Get the value of a specific header.
 * @param key Header name (case-insensitive).
 * @return Header value, or empty string if not present.
 */
std::string Response::get_header_value(const std::string &key) const {
    auto it = headers.find(key);
    return (it == headers.end()) ? std::string{} : it->second;
}

/**
 * @brief Set the response body and Content-Type header.
 * @param s Response body content.
 * @param content_type MIME type of the response body.
 */
void Response::set_content(const std::string &s, const std::string &content_type) {
    body = s;
    headers["Content-Type"] = content_type;
}

/**
 * @brief Set a redirect response.
 * @param url URL to redirect to.
 * @param st HTTP status code (default is 302 Found).
 */
void Response::set_redirect(const std::string &url, int st) {
    status = st;
    headers["location"] = url;
}

/**
 * @brief Get the Content-Length of the response body.
 * @return Number of bytes if Content-Length is valid, std::nullopt otherwise.
 */
std::optional<size_t> Response::content_length() const {
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
 * @brief Get the Content-Type of the response.
 * @return MIME type string, or empty if not present.
 */
std::string Response::content_type() const {
    return get_header_value("content-type");
}

/**
 * @brief Serialize the full HTTP response to string.
 * @return Complete HTTP response string including headers and body.
 */
std::string Response::to_string() const {
    std::ostringstream ss;

    auto reason = reason_phrase(status);

    ss << version << " " << status << " " << reason << "\r\n";

    for (const auto &h: headers) {
        ss << h.first << ": " << h.second << "\r\n";
    }

    if (!body.empty() && !headers.contains("content-length")) {
        ss << "Content-Length: " << body.size() << "\r\n";
    }

    ss << "\r\n";

    if (!body.empty())
        ss << body;

    return ss.str();
}

/**
 * @brief Create a text/plain response.
 * @param s Body content.
 * @param st HTTP status code (default 200 OK).
 * @param charset Character set (default utf-8).
 * @return Response object.
 */
Response Response::text(const std::string& s, int st, const std::string &charset) {
    Response r;
    r.status = st;
    r.set_content(s, std::string("text/plain; charset=") + charset);
    return r;
}

/**
 * @brief Create an application/json response.
 * @param s JSON string.
 * @param st HTTP status code (default 200 OK).
 * @return Response object.
 */
Response Response::json(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "application/json");
    return r;
}

/**
 * @brief Create an HTML response.
 * @param s HTML content.
 * @param st HTTP status code (default 200 OK).
 * @return Response object.
 */
Response Response::html(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "text/html; charset=utf-8");
    return r;
}

/**
 * @brief Create a 404 Not Found response with optional message.
 * @param what Optional message included in the HTML body.
 * @return Response object.
 */
Response Response::not_found(const std::string &what) {
    Response r;
    r.status = NotFound_404;
    r.set_content(
        "<html><body><h1>404 Not Found</h1><p>" + what + "</p></body></html>",
        "text/html"
    );
    return r;
}

/**
 * @brief Create a 400 Bad Request response with JSON message.
 * @param message Error message.
 * @return Response object.
 */
Response Response::bad_request(const std::string &message) {
    Response r;
    r.status = BadRequest_400;
    r.set_content(R"({"error":")" + message + "\"}", "application/json");
    return r;
}

/**
 * @brief Set or overwrite a header.
 * @param key Header name.
 * @param value Header value.
 */
void Response::set_header(const std::string& key, const std::string& value) {
    headers[key] = value;
}

/**
 * @brief Get the standard reason phrase for a given status code.
 * @param st HTTP status code.
 * @return Reason phrase as string_view. Empty string if unknown.
 */
std::string_view Response::reason_phrase(int st) noexcept {
    switch (st) {
        case OK_200: return "OK";
        case Found_302: return "Found";
        case BadRequest_400: return "Bad Request";
        case NotFound_404: return "Not Found";
        case InternalServerError_500: return "Internal Server Error";
        default: return "";
    }
}

/**
 * @brief Serialize only the HTTP headers to a string.
 * @return HTTP response headers as string, ending with CRLF.
 */
std::string Response::to_string_header() const {
    std::string out;

    out += version;
    out += " ";
    out += std::to_string(status);
    out += " ";
    out += reason_phrase(status);
    out += "\r\n";

    for (auto & [name, value] : headers) {
        out += name;
        out += ": ";
        out += value;
        out += "\r\n";
    }

    out += "\r\n";
    return out;
}
