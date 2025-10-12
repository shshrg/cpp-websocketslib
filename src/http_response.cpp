#include "http_response.h"

#include <fstream>

namespace fs = std::filesystem;

bool Response::has_header(std::string_view key) const {
    return headers.contains(key);
}

std::string Response::get_header_value(std::string_view key, size_t id) const {
    auto [it, end] = headers.equal_range(key);
    for (size_t i = 0; it != end; ++it, ++i) {
        if (i == id)
            return it->second;
    }
    return "";
}

size_t Response::get_header_value_count(std::string_view key) const {
    auto [it, end] = headers.equal_range(key);
    return std::distance(it, end);
}

void Response::set_content(const char *s, size_t n, std::string_view content_type) {
    body.assign(s, n);
    headers.emplace("Content-Type", content_type);
}

void Response::set_content(const std::string &s, std::string_view content_type) {
    set_content(s.data(), s.size(), content_type);
}

void Response::set_content(std::string &&s, std::string_view content_type) {
    body = std::move(s);
    headers.emplace("Content-Type", content_type);
}

void Response::set_redirect(std::string_view url, int st) {
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

    ss << version << " " << status << " " << reason << '\n';


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


Response Response::text(std::string s, int st, std::string_view charset) {
    Response r;
    r.status = st;
    r.set_content(std::move(s), std::string("text/plain; charset=") + std::string(charset));
    return r;
}

Response Response::json(std::string s, int st) {
    Response r;
    r.status = st;
    r.set_content(std::move(s), "application/json");
    return r;
}

Response Response::html(std::string s, int st) {
    Response r;
    r.status = st;
    r.set_content(std::move(s), "text/html; charset=utf-8");
    return r;
}


Response Response::not_found(std::string_view what) {
    Response r;
    r.status = NotFound_404;
    r.set_content(
        std::string("<html><body><h1>404 Not Found</h1><p>") + std::string(what) + "</p></body></html>",
        "text/html"
    );
    return r;
}

Response Response::bad_request(std::string_view message) {
    Response r;
    r.status = BadRequest_400;
    r.set_content(
        std::string(R"({"error":")") + std::string(message) + "\"}",
        "application/json"
    );
    return r;
}


static std::string ext_type(std::string_view ext) {
    if (!ext.empty() && ext.front() == '.') ext.remove_prefix(1);

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

    auto it = k.find(std::string(ext));
    return (it != k.end()) ? it->second : std::string("application/octet-stream");
}

static bool read_file(const fs::path &p, std::string &out) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return false;

    ifs.seekg(0, std::ios::end);
    std::streamsize size = ifs.tellg();
    if (size < 0) return false;

    out.resize(size);
    ifs.seekg(0, std::ios::beg);

    if (size > 0) ifs.read(out.data(), size);
    return static_cast<bool>(ifs) || size == 0;
}

static std::optional<fs::path> join_root(const fs::path &root, std::string_view url_path) {
    for (unsigned char c: url_path) {
        if (c == '\0') return std::nullopt;
    }

    while (!url_path.empty() && (url_path.front() == '/' || url_path.front() == '\\'))
        url_path.remove_prefix(1);

    fs::path joined = root / url_path;

    std::error_code ec;
    auto canon_root = fs::weakly_canonical(root, ec);
    if (ec) return std::nullopt;

    auto canon_joined = fs::weakly_canonical(joined, ec);
    if (ec) return std::nullopt;

    const auto &r = canon_root.native();
    const auto &j = canon_joined.native();

    if (j.size() < r.size() || j.compare(0, r.size(), r) != 0)
        return std::nullopt;

    return canon_joined;
}

Response Response::serve_static(const std::filesystem::path &doc_root, std::string_view url_path) {
    if (url_path.empty() || url_path == "/")
        url_path = "/index.html";


    auto joined = join_root(doc_root, url_path);
    if (!joined) return not_found("Invalid path");

    std::string buf;
    if (!read_file(*joined, buf)) {
        return not_found("File not found");
    }
    Response r;
    r.status = OK_200;
    r.set_content(std::move(buf), ext_type(joined->extension().string()));
    return r;
}
