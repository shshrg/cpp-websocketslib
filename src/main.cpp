#include "server.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <sstream>

int main(int argc, char *argv[]) {
    unsigned short port = 8080;
    size_t num_threads = 8;
    bool use_ssl = false;

    // port
    if (argc >= 2) {
        try {
            port = static_cast<unsigned short>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    // threads
    if (argc >= 3) {
        try {
            num_threads = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid thread count: " << argv[2] << "\n";
            return 1;
        }
    }

    // ssl
    if (argc >= 4) {
        std::string arg = argv[3];
        if (arg == "ssl") {
            use_ssl = true;
        } else {
            std::cerr << "Invalid argument: " << arg << "\n";
            std::cerr << "Usage: " << argv[0] << " [port] [threads] [ssl]\n";
            return 1;
        }
    }

    asio::io_context io_context;
    auto address = asio::ip::make_address("0.0.0.0");

    Server server(io_context, address, port, use_ssl);

    server.Get("/hello", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a get method from async");
    });

    server.Post("/hello", [](const Request &request) {
        return Response::text("This was a post method from sync");
    });

    server.Get("/api/files", [](const Request &req) {
        namespace fs = std::filesystem;

        fs::path root = "./www"; // same as MountStatic("/", "./www")

        std::vector<std::string> urls;
        std::error_code ec;

        if (!fs::exists(root, ec) || ec) {
            return Response::not_found("www root not found");
        }

        for (auto const &entry: fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;

            fs::path rel = fs::relative(entry.path(), root, ec);
            if (ec) continue;

            // this becomes the URL the browser will use
            std::string web_path = "/" + rel.generic_string(); // e.g. "/docs/report.pdf"
            urls.push_back(std::move(web_path));
        }

        // Build simple JSON: ["...","...",...]
        std::ostringstream oss;
        oss << "[\n";
        for (size_t i = 0; i < urls.size(); ++i) {
            oss << "  \"";
            for (char c: urls[i]) {
                if (c == '\"') oss << "\\\"";
                else oss << c;
            }
            oss << "\"";
            if (i + 1 < urls.size()) oss << ",";
            oss << "\n";
        }
        oss << "]\n";

        Response res = Response::text(oss.str());
        res.set_header("content-type", "application/json");
        return res;
    });

    server.MountStatic("/", "./www");
    server.MountStatic("/assets/", "./www/assets");
    server.MountStatic("/assets/ui", "./www/assets/ui");

    server.WebSocketRouter("/chat")
            .on_open([](const auto &ws) {
                std::cout << "WebSocket opened\n";
                ws->send_text_async("Welcome!");
            })
            .on_message([](const auto &ws, std::string_view msg) {
                std::cout << "WebSocket message received\n";
                ws->send_text_async("Echo: " + std::string(msg));
            })
            .on_close([](const auto &ws, uint16_t code, std::string_view reason) {
                std::cout << "WebSocket closed with " << code
                        << " reason: " << reason << "\n";
            });

    server.start(num_threads);
    std::cout << "Server started on port " << port << "\n";
    auto test_func = []() {
        std::cout << "Task sent using post_task" << std::endl;
    };

    server.post_task(test_func);

    std::cin.get();
    server.stop();

    std::cout << "Server stopped.\n";

    return 0;
}
