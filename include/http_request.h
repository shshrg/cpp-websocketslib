#ifndef WEBSOCKETLIB_HTTP_REQUEST_H
#define WEBSOCKETLIB_HTTP_REQUEST_H
#include <chrono>
#include <functional>
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

inline std::string_view method_name(Method m) noexcept {
    static constexpr std::string_view names[] = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
    };
    const auto index = static_cast<size_t>(m);
    return index < std::size(names) ? names[index] : "UNKNOWN";
}


struct CaseInsensitiveHash {
    using is_transparent = void;

    size_t operator()(std::string_view s) const noexcept {
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

    bool operator()(std::string_view s1, std::string_view s2) const noexcept {
        if (s1.size() != s2.size())
            return false;

        for (size_t i = 0; i < s1.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s1[i])) != std::tolower(static_cast<unsigned char>(s2[i])))
                return false;
        }
        return true;
    }
};

using Headers = std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;
using Params = std::multimap<std::string, std::string>;


struct Request {
    Method method = Method::UNKNOWN;
    std::string path;
    std::string version;
    std::string target;


    Headers headers;
    Params query_params;
    std::string body;


    bool has_header(std::string_view key) const;
    std::string get_header_value(std::string_view key, size_t id = 0) const;
    size_t get_header_value_count(std::string_view key) const;

    bool has_param(std::string_view key) const;
    std::string get_param_value(std::string_view key, size_t id = 0) const;
    size_t get_param_value_count(std::string_view key) const;

    std::string to_string() const;
    std::optional<size_t> content_length() const;
    std::string content_type() const;
};

inline bool has_content_type(const Request &r, std::string_view sub_type) {
    auto ct = r.content_type();
    auto pos = ct.find(';');

    if (pos != std::string::npos)
        ct.resize(pos);

    return ct == sub_type;
}

#endif //WEBSOCKETLIB_HTTP_REQUEST_H
