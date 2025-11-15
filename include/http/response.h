#ifndef WEBSOCKETLIB_HTTP_RESPONSE_H
#define WEBSOCKETLIB_HTTP_RESPONSE_H
#include "request.h"
#include "status_codes.h"
#include <filesystem>

namespace fs = std::filesystem;

struct Response {
    std::string version = "HTTP/1.1";
    int status = OK_200;
    Headers headers;
    std::string body;

    fs::path sendfile_path{};
    size_t sendfile_size = 0;


    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key) const;

    void set_content(const std::string &s, const std::string &content_type);
    void set_redirect(const std::string &url, int st = Found_302);

    void set_header(const std::string &key, const std::string &value);

    std::optional<size_t> content_length() const;
    std::string content_type() const;

    static Response text(const std::string& s, int st = OK_200, const std::string &charset = "utf-8");
    static Response json(const std::string& s, int st = OK_200);
    static Response html(const std::string& s, int st = OK_200);
    static Response not_found(const std::string &what = "Not Found");
    static Response bad_request(const std::string &message = "Bad Request");
    static std::string_view reason_phrase(int st) noexcept;

    std::string to_string() const;
};


#endif //WEBSOCKETLIB_HTTP_RESPONSE_H
