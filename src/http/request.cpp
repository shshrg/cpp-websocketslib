#include "http/request.h"
#include <sstream>


bool Request::has_header(const std::string &key) const {
    return headers.contains(key);
}

std::string Request::get_header_value(const std::string &key) const {
    auto it = headers.find(key);
    return (it == headers.end()) ? std::string{} : it->second;
}


bool Request::has_param(const std::string &key) const {
    return query_params.contains(key);
}

std::string Request::get_param_value(const std::string &key, size_t id) const {
    auto [it, end] = query_params.equal_range(key);
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

size_t Request::get_param_value_count(const std::string &key) const {
    auto [it, end] = query_params.equal_range(key);
    return std::distance(it, end);
}


std::optional<size_t> Request::content_length() const {
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


std::string Request::content_type() const {
    return get_header_value("content-type");
}

std::string Request::method_str(Method m) noexcept {
    static const std::string names[] = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
    };
    const auto index = static_cast<size_t>(m);
    return index < std::size(names) ? names[index] : "UNKNOWN";
}

Method Request::method_enum(const std::string &s) noexcept {
    static const std::unordered_map<std::string, Method> names = {
        {"GET", Method::GET},
        {"POST", Method::POST},
        {"PUT", Method::PUT},
        {"DELETE", Method::DELETE},
        {"PATCH", Method::PATCH},
        {"HEAD", Method::HEAD},
        {"OPTIONS", Method::OPTIONS}
    };

    auto it = names.find(s);
    return (it == names.end()) ? Method::UNKNOWN : it->second;

}


bool Request::has_content_type(const std::string &sub_type) const {
    auto ct = content_type();
    auto pos = ct.find(';');

    if (pos != std::string::npos)
        ct.resize(pos);

    return ct == sub_type;
}

std::string Request::to_string() const {
    std::ostringstream ss;

    ss << method_str(method) << " " << target << " " << version << "\r\n";

    for (const auto &h: headers) {
        ss << h.first << ": " << h.second << "\r\n";
    }

    if (!body.empty() && !headers.contains("content-length")) {
        ss << "Content-Length: " << body.size() << "\r\n";
    }

    ss << "\r\n";

    ss << body;

    return ss.str();
}
