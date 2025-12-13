#ifndef WEBSOCKETLIB_UTILS_H
#define WEBSOCKETLIB_UTILS_H

#include "request.h"
#include "hash/sha1_wrapper.h"

/**
 * @file utils.h
 * @brief Utility functions for HTTP and WebSocket handling.
 *
 * This header provides helper functions for:
 * - Base64 encoding
 * - WebSocket accept key generation
 * - Building HTTP 101 Switching Protocols responses
 * - String trimming
 * - Parsing HTTP request targets and full requests
 */

/**
 * @brief Encode binary data into Base64 string.
 * @param src Pointer to input bytes.
 * @param len Length of input data.
 * @return Base64-encoded string.
 */
inline std::string base64_encode(const unsigned char *src, size_t len) {
    static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    unsigned val = 0;
    int valb = -6;
    for (size_t i = 0; i < len; ++i) {
        val = (val << 8) | src[i];
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

/**
 * @brief Generate a WebSocket Sec-WebSocket-Accept key from client key.
 * @param client_key Sec-WebSocket-Key sent by client.
 * @return Base64-encoded SHA1 hash suitable for WebSocket handshake.
 */
inline std::string ws_accept_key(std::string_view client_key) {
    static constexpr char kMagic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string concat;
    concat.reserve(client_key.size() + sizeof(kMagic) - 1);
    concat.append(client_key.data(), client_key.size());
    concat.append(kMagic);

    Chocobo1::SHA1 hash;
    hash.addData(concat.data(), concat.size());
    hash.finalize();
    auto digest = hash.toArray(); // std::array<unsigned char, 20>

    return base64_encode(digest.data(), digest.size());
}

/**
 * @brief Build an HTTP 101 Switching Protocols response for WebSocket upgrade.
 * @param accept_key Sec-WebSocket-Accept key computed from client key.
 * @return Response object representing a 101 Switching Protocols message.
 */
inline Response build_101_response(std::string_view accept_key) {
    Response resp;
    resp.status = SwitchingProtocol_101;
    resp.set_header("Upgrade", "websocket");
    resp.set_header("Connection", "Upgrade");
    resp.set_header("Sec-Websocket-Accept", accept_key.data());
    return resp;
}

/**
 * @brief Remove leading whitespace from a string.
 * @param s Input string.
 * @return String with leading whitespace removed.
 */
inline std::string ltrim(std::string s) {
    s.erase(s.begin(),
            std::ranges::find_if(s, [](unsigned char c) { return !std::isspace(c); }));
    return s;
}

/**
 * @brief Remove trailing whitespace from a string.
 * @param s Input string.
 * @return String with trailing whitespace removed.
 */
inline std::string rtrim(std::string s) {
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
        s.end());
    return s;
}

/**
 * @brief Remove leading and trailing whitespace from a string.
 * @param s Input string.
 * @return Trimmed string.
 */
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

/**
 * @brief Split an HTTP request target into path and query parameters.
 * @param target Full request target, e.g., "/path?key=value&key2=value2".
 * @param path Output path component.
 * @param query_params Output query parameters.
 */
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

/**
 * @brief Parse a raw HTTP request string into a Request object.
 * @param data Raw HTTP request text including headers and first line.
 * @param req Output Request object.
 *
 * The function extracts:
 * - HTTP method
 * - Target and query parameters
 * - HTTP version
 * - Headers
 */
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

#endif // WEBSOCKETLIB_UTILS_H
