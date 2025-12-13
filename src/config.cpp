#include "config.h"
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

/**
 * @file config.cpp
 * @brief Implementation of configuration file parsing for the HTTP server.
 *
 * This file provides helper functions and the `load_config` function to parse an INI-like
 * configuration file. It fills a `ServerConfig` structure with server, SSL, and static
 * mount settings. Lines starting with '#' are treated as comments. Unknown keys are
 * warned but ignored. Supports boolean, unsigned short, and size_t parsing.
 *
 * Example usage:
 * @code
 * try {
 *     ServerConfig cfg = load_config("server.conf");
 *     std::cout << "Server running at " << cfg.address << ":" << cfg.port << "\n";
 * } catch (const std::exception &e) {
 *     std::cerr << "Failed to load config: " << e.what() << "\n";
 * }
 * @endcode
 */

/**
 * @brief Trim whitespace from both ends of a string.
 * @param s Input string.
 * @return Trimmed string.
 */
static std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;

    return s.substr(start, end - start);
}

/**
 * @brief Parse a string into a boolean value.
 * Accepts true/yes/on/1 for true, false/no/off/0 for false.
 * @param s Input string.
 * @param out Output boolean.
 * @return true if successfully parsed, false otherwise.
 */
static bool parse_bool(const std::string &s, bool &out) {
    std::string v;
    v.reserve(s.size());
    for (char c: s) v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (v == "true" || v == "yes" || v == "on" || v == "1") {
        out = true;
        return true;
    }
    if (v == "false" || v == "no" || v == "off" || v == "0") {
        out = false;
        return true;
    }
    return false;
}

/**
 * @brief Parse a string into an unsigned short value.
 * @param s Input string.
 * @param out Output value.
 * @return true if successfully parsed, false otherwise.
 */
static bool parse_unsigned_short(const std::string &s, unsigned short &out) {
    try {
        unsigned long v = std::stoul(s);
        if (v > 65535) return false;
        out = static_cast<unsigned short>(v);
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Parse a string into a size_t value.
 * @param s Input string.
 * @param out Output value.
 * @return true if successfully parsed, false otherwise.
 */
static bool parse_size_t(const std::string &s, std::size_t &out) {
    try {
        unsigned long long v = std::stoull(s);
        out = static_cast<std::size_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Load server configuration from a file.
 *
 * Parses an INI-like configuration file and populates a ServerConfig structure.
 * Supported sections:
 * - [server]: address, port, threads, www_root
 * - [ssl]: use_ssl, cert_file, key_file
 * - [static]: mount <url_prefix> <root_path>
 *
 * Lines starting with '#' are ignored as comments. Unknown keys produce a warning.
 *
 * @param filename Path to the configuration file.
 * @return ServerConfig populated with values from the file.
 * @throws std::runtime_error if the file cannot be opened or contains invalid values.
 */
ServerConfig load_config(const std::string &filename) {
    ServerConfig cfg;

    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    std::string section;
    std::string line;
    size_t line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        std::string raw = trim(line);

        if (raw.empty() || raw[0] == '#') {
            continue; // skip comments
        }

        // Section header
        if (raw.front() == '[' && raw.back() == ']') {
            section = raw.substr(1, raw.size() - 2);
            section = trim(section);
            continue;
        }

        // key = value
        auto pos = raw.find('=');
        if (pos == std::string::npos) {
            std::cerr << "Warning: bad line " << line_no << " in " << filename
                      << ": " << raw << "\n";
            continue;
        }

        std::string key = trim(raw.substr(0, pos));
        std::string value = trim(raw.substr(pos + 1));

        // ---- Handle [server] section ----
        if (section == "server") {
            if (key == "address") {
                cfg.address = value;
            } else if (key == "port") {
                unsigned short p;
                if (!parse_unsigned_short(value, p)) {
                    throw std::runtime_error("Invalid port at line " + std::to_string(line_no));
                }
                cfg.port = p;
            } else if (key == "threads") {
                std::size_t t;
                if (!parse_size_t(value, t)) {
                    throw std::runtime_error("Invalid threads at line " + std::to_string(line_no));
                }
                cfg.threads = t;
            } else if (key == "www_root") {
                cfg.www_root = value;
            } else {
                std::cerr << "Warning: unknown key in [server]: " << key << " (line " << line_no << ")\n";
            }
        }
        // ---- Handle [ssl] section ----
        else if (section == "ssl") {
            if (key == "use_ssl") {
                bool b;
                if (!parse_bool(value, b)) {
                    throw std::runtime_error("Invalid bool for use_ssl at line " + std::to_string(line_no));
                }
                cfg.use_ssl = b;
            } else if (key == "cert_file") {
                cfg.ssl_cert_file = value;
            } else if (key == "key_file") {
                cfg.ssl_key_file = value;
            } else {
                std::cerr << "Warning: unknown key in [ssl]: " << key << " (line " << line_no << ")\n";
            }
        }
        // ---- Handle [static] section ----
        else if (section == "static") {
            if (key == "mount") {
                std::istringstream iss(value);
                std::string prefix, path;
                if (!(iss >> prefix >> path)) {
                    throw std::runtime_error("Invalid mount spec at line " + std::to_string(line_no));
                }

                StaticMount m;
                m.url_prefix = prefix;
                m.root = path;
                cfg.mounts.push_back(std::move(m));
            } else {
                std::cerr << "Warning: unknown key in [static]: " << key << " (line " << line_no << ")\n";
            }
        } else {
            std::cerr << "Warning: key outside known section at line " << line_no
                      << ": " << key << "\n";
        }
    }

    return cfg;
}
