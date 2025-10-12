#ifndef WEBSOCKETLIB_HTTP_RESPONSE_H
#define WEBSOCKETLIB_HTTP_RESPONSE_H
#include "http_request.h"
#include "status_codes.h"
#include <filesystem>

inline std::string_view reason_phrase(int status) noexcept {
    switch (status) {
        case OK_200: return "OK";
        case Found_302: return "Found";
        case BadRequest_400: return "Bad Request";
        case NotFound_404: return "Not Found";
        case InternalServerError_500: return "Internal Server Error";
        default: return "";
    }
}

struct Response {
    std::string version = "HTTP/1.1";
    int status = OK_200;
    Headers headers;
    std::string body;


    bool has_header(std::string_view key) const;
    std::string get_header_value(std::string_view key, size_t id = 0) const;
    size_t get_header_value_count(std::string_view key) const;

    void set_content(const char *s, size_t n, std::string_view content_type);
    void set_content(const std::string &s, std::string_view content_type);
    void set_content(std::string &&s, std::string_view content_type);
    void set_redirect(std::string_view url, int st = Found_302);

    std::optional<size_t> content_length() const;
    std::string content_type() const;

    static Response text(std::string s, int st = OK_200, std::string_view charset = "utf-8");
    static Response json(std::string s, int st = OK_200);
    static Response html(std::string s, int st = OK_200);
    static Response not_found(std::string_view what = "Not Found");
    static Response bad_request(std::string_view message = "Bad Request");
    static Response serve_static(const std::filesystem::path &doc_root,
                                 std::string_view url_path);

    std::string to_string() const;
};


#endif //WEBSOCKETLIB_HTTP_RESPONSE_H
