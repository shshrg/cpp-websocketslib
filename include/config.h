#ifndef WEBSOCKETLIB_CONFIG_H
#define WEBSOCKETLIB_CONFIG_H

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * @file config.h
 * @brief Server configuration structures and loader.
 *
 * This header defines the configuration structures for the WebSocket/HTTP server,
 * including static mounts, network settings, SSL options, and thread counts.
 */

/**
 * @brief Represents a static file mount point.
 *
 * A static mount maps a URL prefix to a local filesystem directory.
 */
struct StaticMount {
    std::string url_prefix; ///< URL prefix (e.g., "/static")
    fs::path root;          ///< Local directory that is served under this prefix
};

/**
 * @brief Configuration structure for the HTTP/WebSocket server.
 */
struct ServerConfig {
    std::string address = "127.0.0.1";  ///< IP address to bind the server to
    unsigned short port = 8080;         ///< TCP port
    std::size_t threads = 8;            ///< Number of worker threads
    bool use_ssl = false;               ///< Whether to enable SSL/TLS
    fs::path www_root = "./www";        ///< Root directory for serving static files

    fs::path ssl_cert_file = "certs/localhost.crt"; ///< Path to SSL certificate
    fs::path ssl_key_file  = "certs/localhost.key"; ///< Path to SSL private key

    std::vector<StaticMount> mounts;    ///< List of additional static mounts
};

/**
 * @brief Load a server configuration from a file.
 * @param filename Path to the configuration file.
 * @return ServerConfig object populated with values from the file.
 *
 * This function reads configuration settings from a file and returns a fully
 * initialized ServerConfig structure. If the file cannot be read, default
 * values are used.
 */
ServerConfig load_config(const std::string& filename);

#endif // WEBSOCKETLIB_CONFIG_H
