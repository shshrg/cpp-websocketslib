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
    std::string method;
    std::string path; // normalized path e.g. /search
    // std::string matched_route; // use regex in routing?
    Headers headers;
    Params params; // store query parameters
    // Headers trailers; // additional headers, like Digest or Checksum
    // used for example, after uploading a big body

    std::string body;

    // Server ip and port that accepts the connection
    std::string local_addr;
    int local_port = -1;

    // std::string remote_addr; // client ip
    // int remote_port = -1; // client port

    // for server
    std::string version; // how to parse the response
    std::string target; // include path + query params
    // Ranges ranges // parse ranges in headers: bytes=0-99
    // MultipartFormData form; // parses the files in forms or when uploading
    // std::smatch matches; // use regex in routing?
    // A callback to check client connection status
    // std::function<bool()> is_connection_closed = []() { return true; };
    // std::unordered_map<std::string, std::string> path_params; vars in routing?

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    const SSL *ssl = nullptr;
#endif


    // Headers API
    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key, const char *def = "",
                               size_t id = 0) const;

    size_t get_header_value_u64(const std::string &key, size_t def = 0,
                              size_t id = 0) const;

    size_t get_header_value_count(const std::string &key) const;

    // Do we use middleware to annotate Request?
    // void set_header(const std::string &key, const std::string &val);

    // Do we use trailers?
    // bool has_trailer(const std::string &key) const;
    // std::string get_trailer_value(const std::string &key, size_t id = 0) const;
    // size_t get_trailer_value_count(const std::string &key) const;

    // Query/Form params API
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

    // Do we need content provider?
};


#endif //WEBSOCKETLIB_HTTP_REQUEST_H
