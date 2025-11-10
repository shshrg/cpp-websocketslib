#ifndef WEBSOCKETLIB_UTILS_H
#define WEBSOCKETLIB_UTILS_H

#include "request.h"
#include <openssl/sha.h>

inline std::string ws_accept_key(std::string_view client_key) {
    static constexpr char kMagic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string concat;
    concat.reserve(client_key.size() + sizeof(kMagic) - 1);
    concat.append(client_key.data(), client_key.size());
    concat.append(kMagic);

    unsigned char sha1[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(concat.data()), concat.size(), sha1);

    std::string out;
    out.resize(4 * ((SHA_DIGEST_LENGTH + 2) / 3));
    int n = EVP_EncodeBlock(
        reinterpret_cast<unsigned char *>(&out[0]),
        sha1, SHA_DIGEST_LENGTH);
    out.resize(n);
    return out;
}

inline Response build_101_response(std::string_view accept_key) {
    Response resp;
    resp.status = SwitchingProtocol_101;
    resp.set_header("Upgrade", "websocket");
    resp.set_header("Connection", "Upgrade");
    resp.set_header("Sec-Websocket-Accept", accept_key.data());
    return resp;
}


inline std::string ltrim(std::string s) {
    s.erase(s.begin(),
            std::ranges::find_if(s, [](unsigned char c) { return !std::isspace(c); }));
    return s;
}

inline std::string rtrim(std::string s) {
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
        s.end());
    return s;
}

inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }


inline void split_target(const std::string &target, std::string &path, Params &query_params) {
    query_params.clear();

    const auto qm = target.find('?');
    if (qm == std::string::npos) {
        path = target;
        return;
    }

    path = target.substr(0, qm);
    std::string query = target.substr(qm + 1);

    size_t start = 0;
    while (start <= query.size()) {
        size_t param_sep = query.find('&', start);
        std::string param = (param_sep == std::string::npos)
                                ? query.substr(start)
                                : query.substr(start, param_sep - start);

        if (!param.empty()) {
            size_t eq = param.find('=');
            std::string key = param.substr(0, eq);
            std::string value = (eq == std::string::npos)
                                    ? std::string{}
                                    : param.substr(eq + 1);
            query_params.emplace(std::move(key), std::move(value));
        }
        if (param_sep == std::string::npos) break;
        start = param_sep + 1;
    }
}

inline void parse_http_request(const std::string &data, Request &req) {
    std::istringstream stream(data);
    std::string line;

    std::getline(stream, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream firstline(line);
    std::string method_str;
    firstline >> method_str >> req.target >> req.version;
    req.method = Request::method_enum(method_str);

    split_target(req.target, req.path, req.query_params);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;

        size_t col = line.find(':');
        if (col == std::string::npos) continue;

        std::string key = trim(line.substr(0, col));

        size_t pos = col + 1;
        if (pos < line.size() && line[pos] == ' ') ++pos;
        std::string value = trim(line.substr(pos));

        req.headers[key] = std::move(value);
    }
}


#endif //WEBSOCKETLIB_UTILS_H
