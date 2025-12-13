#define _WIN32_WINNT 0x0A00
#include <iostream>
#include "httplib.h"

int main() {
    httplib::Server svr;

    svr.Get("/hello", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("Hello benchmark\n", "text/plain");
    });

    svr.set_mount_point("/static", "./www/static");

    // Add /ping route too
    svr.Get("/ping", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("OK\n", "text/plain");
        res.set_header("Connection", "close");
    });

    svr.Post("/echo", [](const httplib::Request &req, httplib::Response &res) {
        res.set_content(req.body, "application/octet-stream");
    });

    std::cout << "cpp-httplib benchmark server listening on http://0.0.0.0:8080/hello\n";

    if (!svr.listen("0.0.0.0", 8081)) {
        std::cerr << "Failed to listen on port 8080\n";
        return 1;
    }
    return 0;
}
