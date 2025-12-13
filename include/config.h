#ifndef WEBSOCKETLIB_CONFIG_H
#define WEBSOCKETLIB_CONFIG_H
#include <filesystem>
#include <string>
#include <vector>


namespace fs = std::filesystem;

struct StaticMount {
    std::string url_prefix;
    fs::path root;
};

struct ServerConfig {
    std::string address = "127.0.0.1";
    unsigned short port = 8080;
    std::size_t threads = 8;
    bool use_ssl = false;
    fs::path www_root = "./www";

    fs::path ssl_cert_file = "certs/localhost.crt";
    fs::path ssl_key_file  = "certs/localhost.key";

    std::vector<StaticMount> mounts;
};

ServerConfig load_config(const std::string& filename);


#endif //WEBSOCKETLIB_CONFIG_H