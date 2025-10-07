#ifndef WEBSOCKETLIB_HTTP_REQUEST_H
#define WEBSOCKETLIB_HTTP_REQUEST_H
#include <chrono>
#include <functional>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>


// using Range = std::pair<ssize_t, ssize_t>;
// using Ranges = std::vector<Range>;
using Headers = std::unordered_multimap<std::string, std::string/*, CaseInsensitiveHash, CaseInsensitiveEqual */>;
using Params = std::multimap<std::string, std::string>;


struct Request {
    std::string method;  // TODO: make enum of methods
    std::string path; // normalized path e.g. /search
    std::string version; // how to parse the response
    std::string target; // include path + query params


    Headers headers;
    Params query_params; // store query parameters
    std::string body;

    // Server ip and port that accepts the connection
    std::string local_addr;
    int local_port = -1;
    std::string remote_addr; // client ip
    int remote_port = -1; // client port // TODO: check if this is important

    size_t content_length = 0;
    std::string content_type;

    // Headers API
    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key, const char *def = "",
                               size_t id = 0) const;

    bool has_param(const std::string &key) const;
    std::string get_param_value(const std::string &key, size_t id = 0) const;
    size_t get_param_value_count(const std::string &key) const;

    // Content-type helper
    bool is_multipart_form_data() const;

    // Add some new methods?
    std::string to_string() const;

private:
    size_t content_length_ = 0;
    std::chrono::time_point<std::chrono::steady_clock> start_time;

};


#endif //WEBSOCKETLIB_HTTP_REQUEST_H
