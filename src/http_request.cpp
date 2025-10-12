#include "http_request.h"
#include <sstream>


bool Request::has_header(std::string_view key) const {
    return headers.contains(key);
}

std::string Request::get_header_value(std::string_view key, size_t id) const {
    auto [it, end] = headers.equal_range(key);
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

size_t Request::get_header_value_count(std::string_view key) const {
    auto [it, end] = headers.equal_range(key);
    return std::distance(it, end);
}


bool Request::has_param(std::string_view key) const {
    return query_params.contains(std::string(key));
}

std::string Request::get_param_value(std::string_view key, size_t id) const {
    auto [it, end] = query_params.equal_range(std::string(key));
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

size_t Request::get_param_value_count(std::string_view key) const {
    auto [it, end] = query_params.equal_range(std::string(key));
    return std::distance(it, end);
}


std::optional<size_t> Request::content_length() const {
    auto s = get_header_value("Content-Length");
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
    return get_header_value("Content-Type");
}


std::string Request::to_string() const {
    std::ostringstream ss;

    ss << method_name(method) << " " << target << " " << version << '\n';

    for (const auto &h: headers) {
        ss << h.first << ": " << h.second << '\n';
    }

    if (!body.empty() && !headers.contains("Content-Length")) {
        ss << "Content-Length: " << body.size() << '\n';
    }

    ss << '\n';

    ss << body;

    return ss.str();
}
