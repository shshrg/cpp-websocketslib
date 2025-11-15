#include "http/response.h"
#include <fstream>

namespace fs = std::filesystem;

bool Response::has_header(const std::string &key) const {
    return headers.contains(key);
}

std::string Response::get_header_value(const std::string &key) const {
    auto it = headers.find(key);
    return (it == headers.end()) ? std::string{} : it->second;
}

void Response::set_content(const std::string &s, const std::string &content_type) {
    body = s;
    headers["Content-Type"] = content_type;
}

void Response::set_redirect(const std::string &url, int st) {
    status = st;
    headers["location"] = url;
}

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

std::string Response::content_type() const {
    return get_header_value("content-type");
}


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


Response Response::text(const std::string& s, int st, const std::string &charset) {
    Response r;
    r.status = st;
    r.set_content(s, std::string("text/plain; charset=") + std::string(charset));
    return r;
}

Response Response::json(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "application/json");
    return r;
}

Response Response::html(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "text/html; charset=utf-8");
    return r;
}


Response Response::not_found(const std::string &what) {
    Response r;
    r.status = NotFound_404;
    r.set_content(
        std::string("<html><body><h1>404 Not Found</h1><p>") + std::string(what) + "</p></body></html>",
        "text/html"
    );
    return r;
}

Response Response::bad_request(const std::string &message) {
    Response r;
    r.status = BadRequest_400;
    r.set_content(
        std::string(R"({"error":")") + std::string(message) + "\"}",
        "application/json"
    );
    return r;
}

void Response::set_header(const std::string& key, const std::string& value) {
    headers[key] = value;
}

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