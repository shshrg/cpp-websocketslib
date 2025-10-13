#include "http/response.h"
#include <fstream>

namespace fs = std::filesystem;

bool Response::has_header(const std::string &key) const {
    return headers.contains(key);
}

std::string Response::get_header_value(const std::string &key, size_t id) const {
    auto [it, end] = headers.equal_range(key);
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

size_t Response::get_header_value_count(const std::string &key) const {
    auto [it, end] = headers.equal_range(key);
    return std::distance(it, end);
}


void Response::set_content(const std::string &s, const std::string &content_type) {
    body = s;
    headers.emplace("Content-Type", content_type);
}

void Response::set_redirect(const std::string &url, int st) {
    status = st;
    headers.emplace("Location", url);
}

std::optional<size_t> Response::content_length() const {
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

std::string Response::content_type() const {
    return get_header_value("Content-Type");
}


std::string Response::to_string() const {
    std::ostringstream ss;

    auto reason = reason_phrase(status);

    ss << version << " " << status << " " << reason << "\r\n";


    for (const auto &h: headers) {
        ss << h.first << ": " << h.second << "\r\n";
    }

    if (!body.empty() && !headers.contains("Content-Length")) {
        ss << "Content-Length: " << body.size() << "\r\n";
    }

    ss << "\r\n";

    ss << body;

    return ss.str();
}


Response Response::text(const std::string& s, int st, const std::string &charset) {
    Response r;
    r.status = st;
    r.set_content(s, std::string("text/plain; charset=") + std::string(charset));
    return r;
}

Response Response::json(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "application/json");
    return r;
}

Response Response::html(const std::string& s, int st) {
    Response r;
    r.status = st;
    r.set_content(s, "text/html; charset=utf-8");
    return r;
}


Response Response::not_found(const std::string &what) {
    Response r;
    r.status = NotFound_404;
    r.set_content(
        std::string("<html><body><h1>404 Not Found</h1><p>") + std::string(what) + "</p></body></html>",
        "text/html"
    );
    return r;
}

Response Response::bad_request(const std::string &message) {
    Response r;
    r.status = BadRequest_400;
    r.set_content(
        std::string(R"({"error":")") + std::string(message) + "\"}",
        "application/json"
    );
    return r;
}


static std::string ext_type(const std::string &ext) {
    std::string e = ext;
    if (!e.empty() && e.front() == '.')
        e.erase(0, 1);

    for (auto &ch: e) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    static const std::unordered_map<std::string, std::string> k = {
        {"html", "text/html; charset=utf-8"},
        {"htm", "text/html; charset=utf-8"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"mjs", "application/javascript"},
        {"json", "application/json"},
        {"txt", "text/plain; charset=utf-8"},
        {"xml", "application/xml"},
        {"svg", "image/svg+xml"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"wasm", "application/wasm"},
        {"mp4", "video/mp4"}
    };

    auto it = k.find(e);
    return (it != k.end()) ? it->second : std::string("application/octet-stream");
}


static fs::path join_root(const fs::path &root, std::string url_path) {
    for (unsigned char c: url_path) {
        if (c == '\0') return {};
    }

    for (auto &ch: url_path) if (ch == '\\') ch = '/';
    while (!url_path.empty() && url_path.front() == '/') url_path.erase(0, 1);

    std::error_code ec;
    auto canon_root = fs::weakly_canonical(root, ec);
    if (ec) return {};

    auto joined = canon_root / url_path;

    auto canon_joined = fs::weakly_canonical(joined, ec);
    if (ec) return {};

    auto rel = fs::relative(joined, root, ec);

    if (ec || rel.empty()) return {};
    if (*rel.begin() == "..") return {};

    return canon_joined;
}

Response Response::serve_static(const std::filesystem::path &doc_root, const std::string &url_path) {
    auto joined = join_root(doc_root, url_path);
    if (joined.empty()) return not_found("Invalid path");

    std::error_code ec;
    auto stat = fs::symlink_status(joined, ec);
    if (ec || !fs::is_regular_file(stat))
        return not_found("File not found");

    auto sz = fs::file_size(joined, ec);
    if (ec) return not_found("File not found");

    Response r;
    r.status = OK_200;
    r.headers.emplace("Content-Type", ext_type(joined.extension().string()));
    r.headers.emplace("Content-Length", std::to_string(sz));

    r.sendfile_path = joined;
    r.sendfile_size = sz;
    return r;
}

std::string_view Response::reason_phrase(int st) noexcept {
    switch (st) {
        case OK_200: return "OK";
        case Found_302: return "Found";
        case BadRequest_400: return "Bad Request";
        case NotFound_404: return "Not Found";
        case InternalServerError_500: return "Internal Server Error";
        default: return "";
    }
}
