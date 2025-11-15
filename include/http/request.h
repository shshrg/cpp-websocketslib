#ifndef WEBSOCKETLIB_HTTP_REQUEST_H
#define WEBSOCKETLIB_HTTP_REQUEST_H
#include <chrono>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>

enum class Method : uint8_t {
    GET,
    POST,
    PUT,
    DELETE_,
    PATCH,
    HEAD,
    OPTIONS,
    UNKNOWN
};

struct CaseInsensitiveHash {
    size_t operator()(const std::string &s) const noexcept {
        std::string lower;
        lower.reserve(s.size());

        for (const auto &c: s) {
            lower.push_back(std::tolower(static_cast<unsigned char>(c)));
        }

        return std::hash<std::string>()(lower);
    }
};

struct CaseInsensitiveEqual {
    using is_transparent = void;

    bool operator()(const std::string & s1, const std::string & s2) const noexcept {
        if (s1.size() != s2.size())
            return false;

        for (size_t i = 0; i < s1.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s1[i])) != std::tolower(static_cast<unsigned char>(s2[i])))
                return false;
        }
        return true;
    }
};

using Headers = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;
using Params = std::multimap<std::string, std::string>;

struct Request {
    Method method = Method::UNKNOWN;
    std::string path;
    std::string version;
    std::string target;


    Headers headers;
    Params query_params;
    std::string body;


    bool has_header(const std::string & key) const;
    std::string get_header_value(const std::string & key) const;

    bool has_param(const std::string & key) const;
    std::string get_param_value(const std::string & key, size_t id = 0) const;
    size_t get_param_value_count(const std::string & key) const;

    std::string to_string() const;
    std::optional<size_t> content_length() const;
    std::string content_type() const;

    static std::string method_str(Method m) noexcept;
    static Method method_enum(const std::string & s) noexcept;

    bool has_content_type(const std::string & sub_type) const;

    bool is_ws_upgrade() const;
};


#endif //WEBSOCKETLIB_HTTP_REQUEST_H

