#include "gtest/gtest.h"
#include "server.h"
#include "config.h"
#include "http_client_example.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

ServerConfig cfg;

// Helper function to configure and connect a client
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

// Test serving static files
TEST(HTTP_tests, MountStatic_Client) {
    // Create a temporary static folder with index.html
    // Check if GET request returns correct HTML content and headers
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

// Test GET requests for both synchronous and asynchronous handlers
TEST(HTTP_tests, Get_Sync_Async_Client) {
    // Ensure async and sync GET endpoints return correct responses
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

// Test POST requests for both synchronous and asynchronous handlers
TEST(HTTP_tests, Post_Sync_Async_Client) {
    // Ensure async and sync POST endpoints return correct responses
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

// Test unknown route returns 404 Not Found
TEST(HTTP_tests, NotFound_404) {
    // Check that server correctly returns 404 for nonexistent paths
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    make_client(client);

    std::string resp = client.get("/nonexistent");
    EXPECT_NE(resp.find("404 Not Found"), std::string::npos);

    server.stop();
    io.stop();
    server_thread.join();
}

// Test POST request with empty body
TEST(HTTP_tests, Empty_Post_Body) {
    // Ensure server can handle empty POST body correctly
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Post("/echo", [](const Request &request) {
        return Response::text(request.body);  // echo body
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    make_client(client);

    std::string resp = client.post("/echo", "");
    EXPECT_NE(resp.find("200 OK"), std::string::npos);
    EXPECT_NE(resp.find(""), std::string::npos);  // body should be empty

    server.stop();
    io.stop();
    server_thread.join();
}

// Test server handling of large request body
TEST(HTTP_tests, Large_Payload) {
    // Check that server handles 10 MB payload correctly
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Post("/large", [](const Request &request) {
        return Response::text(std::to_string(request.body.size())); // return size
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    make_client(client);

    std::string large_body(10 * 1024 * 1024, 'x'); // 10 MB
    std::string resp = client.post("/large", large_body);
    EXPECT_NE(resp.find("10485760"), std::string::npos); // 10 MB in bytes

    server.stop();
    io.stop();
    server_thread.join();
}

// Test GET request with query parameters
TEST(HTTP_tests, Query_Params) {
    // Ensure server parses query parameters correctly
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/query", [](const Request &request) {
        std::string result;
        for (const auto& [key, value] : request.query_params) {
            result.append(key).append("=").append(value).append("&");
        }
        if (!result.empty()) result.pop_back();
        return Response::text(result);
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    make_client(client);

    std::string resp = client.get("/query?name=test&value=123");

    EXPECT_NE(resp.find("name=test"), std::string::npos);
    EXPECT_NE(resp.find("value=123"), std::string::npos);

    server.stop();
    io.stop();
    server_thread.join();
}

// Test multiple clients hitting the server at the same time
TEST(HTTP_tests, Concurrent_Requests) {
    // Simulate 50 clients sending GET /ping concurrently
    // Ensure all receive "pong"
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/ping", [](const Request &request) {
        return Response::text("pong");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    const int num_clients = 50;
    std::vector<std::thread> clients;
    for (int i = 0; i < num_clients; ++i) {
        clients.emplace_back([&]() {
            HttpClient client(cfg.use_ssl);
            make_client(client);
            std::string resp = client.get("/ping");
            EXPECT_NE(resp.find("pong"), std::string::npos);
        });
    }

    for (auto &t : clients) t.join();

    server.stop();
    io.stop();
    server_thread.join();
}

// Test route with special characters and encoding
TEST(HTTP_tests, Special_Characters_Route) {
    // Ensure server handles URL-encoded paths correctly
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/special/%20test", [](const Request &request) {
        return Response::text("ok");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    make_client(client);

    std::string resp = client.get("/special/%20test");
    EXPECT_NE(resp.find("ok"), std::string::npos);

    server.stop();
    io.stop();
    server_thread.join();
}


#ifdef USE_SSL

// Test SSL connection with valid certificate
TEST(HTTP_tests, SSL_Connection) {
    // Check server responds correctly over SSL
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    cfg.use_ssl = true;

    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/sslping", [](const Request &request) {
        return Response::text("ssl ok");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        HttpClient client(cfg.use_ssl);
        if (!cfg.ssl_cert_file.empty()) {
            client.set_verify_cert_file(cfg.ssl_cert_file);
        }

        make_client(client); // connect to server
        std::string resp = client.get("/sslping");
        EXPECT_NE(resp.find("ssl ok"), std::string::npos);
    } catch (const std::exception &e) {
        FAIL() << "SSL test failed: " << e.what();
    }

    server.stop();
    io.stop();
    server_thread.join();
}

// Test SSL connection with invalid certificate
TEST(HTTP_tests, SSL_Invalid_Cert) {
    // Ensure client fails to connect with wrong certificate
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    cfg.use_ssl = true;

    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Get("/sslping", [](const Request &request) {
        return Response::text("ssl ok");
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        HttpClient client(cfg.use_ssl);
        client.set_verify_cert_file("invalid_cert.pem");
        client.set_verify_cert_file(cfg.ssl_cert_file);

        make_client(client);
        FAIL() << "Expected SSL verification to fail";
    } catch (const std::exception&) {
        SUCCEED();
    }

    server.stop();
    io.stop();
    server_thread.join();
}

// Test server handling large SSL requests
TEST(HTTP_tests, SSL_Large_Payload) {
    // Check server handles large SSL POST payload (5 MB)
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    cfg.use_ssl = true;

    Server server(io, address, cfg.port, cfg.use_ssl);

    server.Post("/ssl_large", [](const Request &request) {
        return Response::text(std::to_string(request.body.size()));
    });

    server.start(cfg.threads);
    std::thread server_thread([&io]() { io.run(); });

    HttpClient client(cfg.use_ssl);
    if (!cfg.ssl_cert_file.empty()) {
        client.set_verify_cert_file(cfg.ssl_cert_file);
    }
    make_client(client);

    std::string large_body(5 * 1024 * 1024, 'x'); // 5 MB
    std::string resp = client.post("/ssl_large", large_body);
    EXPECT_NE(resp.find("5242880"), std::string::npos); // 5 MB in bytes

    server.stop();
    io.stop();
    server_thread.join();
}

#endif // USE_SSL
