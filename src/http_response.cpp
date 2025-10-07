#include "http_response.h"


Response Response::text(std::string s, int status, std::string_view charset) {
    Response r;
    r.status = status;
    r.set_content(std::move(s), std::string("text/plain; charset=") + std::string(charset));
    return r;
}

Response Response::json(std::string s, int status) {
    Response r;
    r.status = status;
    r.set_content(std::move(s), "application/json");
    return r;
}

Response Response::html(std::string s, int status) {
    Response r;
    r.status = status;
    r.set_content(std::move(s), "text/html; charset=utf-8");
    return r;
}


void Response::set_content(const std::string &s, const std::string &content_type) {
    body = s;
    headers.emplace("Content-Type", content_type);
}
void Response::set_content(std::string &&s, const std::string &content_type) {
    body = std::move(s);
    headers.emplace("Content-Type", content_type);
}
void Response::set_content(const char *s, size_t n, const std::string &content_type) {
    body.assign(s, n);
    headers.emplace("Content-Type", content_type);
}