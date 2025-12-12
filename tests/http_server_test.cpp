#include "gtest/gtest.h"
#include "server.h"
#include "config.h"
#include "http_client_example.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

ServerConfig cfg;

HttpClient& make_client(HttpClient& client) {
#ifdef USE_SSL
    if (cfg.use_ssl && !cfg.ssl_cert_file.empty()) {
        client.set_verify_cert_file(cfg.ssl_cert_file);
    }
#endif

    std::string host = cfg.address.empty() ? "127.0.0.1" : cfg.address;
    std::string port = std::to_string(cfg.port);
    client.connect(host, port);

    return client;
}

TEST(HTTP_tests, MountStatic_Client) {
    std::filesystem::path tmpdir = std::filesystem::temp_directory_path() / "server_test_static";
    std::filesystem::create_directories(tmpdir);
    std::filesystem::path index = tmpdir / "index.html";
    {
        std::ofstream ofs(index);
        ofs << "<html><body>OK</body></html>";
    }

    auto address = asio::ip::make_address(cfg.address);
    asio::io_context io;
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.MountStatic("/static", tmpdir);
    server.start(cfg.threads);

    std::thread server_thread([&io]() { io.run(); });

    {
        HttpClient client(cfg.use_ssl);
        make_client(client);

        std::string resp = client.get("/static");

        EXPECT_NE(resp.find("200 OK"), std::string::npos);
        EXPECT_NE(resp.find("Content-Type: text/html"), std::string::npos);
        EXPECT_NE(resp.find("<html><body>OK</body></html>"), std::string::npos);
    }

    server.stop();
    io.stop();
    server_thread.join();

    std::filesystem::remove_all(tmpdir);
}

TEST(HTTP_tests, Get_Sync_Async_Client) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/asyncmethod", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a get method from async");
    });
    server.Get("/syncmethod", [](const Request &request) {
        return Response::text("This was a get method from sync");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    {
        HttpClient client(cfg.use_ssl);
        make_client(client);

        std::string resp_async = client.get("/asyncmethod");
        EXPECT_NE(resp_async.find("This was a get method from async"), std::string::npos);
    }

    {
        HttpClient client(cfg.use_ssl);
        make_client(client);

        std::string resp_sync = client.get("/syncmethod");
        EXPECT_NE(resp_sync.find("This was a get method from sync"), std::string::npos);
    }

    server.stop();
    io.stop();
    server_thread.join();
}

TEST(HTTP_tests, Post_Sync_Async_Client) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Post("/asyncmethod", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a post method from async");
    });
    server.Post("/syncmethod", [](const Request &request) {
        return Response::text("This was a post method from sync");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    {
        HttpClient client(cfg.use_ssl);
        make_client(client);

        std::string resp_post_async = client.post("/asyncmethod", "");
        EXPECT_NE(resp_post_async.find("This was a post method from async"), std::string::npos);
    }

    {
        HttpClient client(cfg.use_ssl);
        make_client(client);

        std::string resp_post_sync = client.post("/syncmethod", "");
        EXPECT_NE(resp_post_sync.find("This was a post method from sync"), std::string::npos);
    }

    server.stop();
    io.stop();
    server_thread.join();
}
