#ifndef WEBSOCKETLIB_HTTP_RESPONSE_H
#define WEBSOCKETLIB_HTTP_RESPONSE_H
#include "http_request.h"
#include "status_codes.h"

struct Response {
    std::string version;
    int status = 200;
    Headers headers;
    std::string body;

    std::string location;

    size_t content_length = 0;


    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key, const char *def = "",
                                   size_t id = 0) const;
    size_t get_header_value_u64(const std::string &key, size_t def = 0,
                                size_t id = 0) const;
    size_t get_header_value_count(const std::string &key) const;

    void set_redirect(const std::string &url, int status = Found_302);

    void set_content(const char *s, size_t n, const std::string &content_type);
    void set_content(const std::string &s, const std::string &content_type);
    void set_content(std::string &&s, const std::string &content_type);


    static Response text(std::string s, int status, std::string_view charset="utf-8");
    static Response json(std::string s, int status);
    static Response html(std::string s, int status);
    // TODO: add not_found, bad_request, serve_static (host static files, for example Frontend), file (download a file)

    std::string to_string() const;

};


#endif //WEBSOCKETLIB_HTTP_RESPONSE_H