#include "gtest/gtest.h"
#include "asio/any_io_executor.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/address.hpp"
#include "asio/stream_file.hpp"
#include "server.h"
#include "config.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>

ServerConfig cfg;

// -----------------helper functions-----------------
std::string connect_request_response(std::string& request, asio::io_context& io) {
    asio::ip::tcp::socket client(io);
    auto address = asio::ip::make_address(cfg.address);
    client.connect(asio::ip::tcp::endpoint(address, cfg.port));
    asio::write(client, asio::buffer(request));
    std::string response;
    for (;;) {
        char buf[1024];
        std::error_code ec;
        std::size_t n = client.read_some(asio::buffer(buf), ec);
        if (ec == asio::error::eof) break;
        if (ec) break;
        response.append(buf, n);
    }
    client.close();
    return response;
}

// --------------------------------------------------
//              HTTP FUNCTIONALITY TESTS
// --------------------------------------------------

TEST(HTTP_tests, MountStatic) {
    std::filesystem::path tmpdir = std::filesystem::temp_directory_path() / std::filesystem::path("server_test_static");
    std::filesystem::create_directories(tmpdir);
    std::filesystem::path index = tmpdir / "index.html";
    {
        std::ofstream ofs(index);
        ofs << "<html><body>OK</body></html>";
    }

    auto address = asio::ip::make_address(cfg.address);
    asio::io_context io;
    Server server(io, address, cfg.port, cfg.use_ssl);
    server.start(1);

    server.MountStatic("/static", tmpdir);

    server.start(cfg.threads);

    std::string req = 
        "GET /static HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    std::string resp = connect_request_response(req, io);

    server.stop();

    EXPECT_NE(resp.find("200 OK"), std::string::npos)
        << "Response did not contain 200 OK: ";
    EXPECT_NE(resp.find("Content-Type: text/html"), std::string::npos)
        << "Content-Type is incorrect";
    EXPECT_NE(resp.find("<html><body>OK</body></html>"), std::string::npos)
        << "Body not returned correctly";

    std::filesystem::remove_all(tmpdir);
}

TEST(HTTP_tests, Get_Sync_Async) {
    auto address = asio::ip::make_address(cfg.address);
    asio::io_context io;
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.start(cfg.threads);

    server.Get("/asyncmethod", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a get method from async");
    });

    server.Get("/syncmethod", [](const Request &request) {
        return Response::text("This was a get method from sync");
    });

    std::string req_get_async = 
        "GET /asyncmethod HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n\r\n";

    std::string resp_get_async = connect_request_response(req_get_async, io);

    EXPECT_NE(resp_get_async.find("This was a get method from async"), std::string::npos)
        << "Get async method did not return expected result";


    std::string req_get_sync = 
        "GET /syncmethod HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    std::string resp_get_sync = connect_request_response(req_get_sync, io);

    server.stop();
    
    EXPECT_NE(resp_get_sync.find("This was a get method from sync"), std::string::npos)
        << "Get sync method did not return expected result";
}


TEST(HTTP_tests, Post_Sync_Async) {
    auto address = asio::ip::make_address(cfg.address);
    asio::io_context io;
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.start(cfg.threads);

    server.Post("/asyncmethod", [](const Request &request) -> asio::awaitable<Response> {
        co_return Response::text("This was a post method from async");
    });

    server.Post("/syncmethod", [](const Request &request) {
        return Response::text("This was a post method from sync");
    });

    std::string req_post_async = 
        "POST /asyncmethod HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n\r\n";

    std::string resp_post_async = connect_request_response(req_post_async, io);

    EXPECT_NE(resp_post_async.find("This was a post method from async"), std::string::npos)
        << "Post async method did not return expected result";


    std::string req_post_sync = 
        "POST /syncmethod HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    std::string resp_post_sync = connect_request_response(req_post_sync, io);

    server.stop();
    
    EXPECT_NE(resp_post_sync.find("This was a post method from sync"), std::string::npos)
        << "Post sync method did not return expected result";
}

// --------------------------------------------------
//                  WEBSOCKET TESTS
// --------------------------------------------------
// ------------------UNDER CONSTRUCTION--------------
// TEST(WS_tests, Handshake_Tests) {
//     auto address = asio::ip::make_address(cfg.address);
//     asio::io_context io;
//     Server server(io, address, cfg.port, cfg.use_ssl);

//     server.start(cfg.threads);

//     server.WebSocketRouter("/chat")
//         .on_open([](const auto &ws) {
//             std::cout << "WebSocket opened\n";
//             ws->send_text_async("Welcome!");
//         })
//         .on_message([](const auto &ws, std::string_view msg) {
//             std::cout << "WebSocket message received\n";
//         })
//         .on_close([](const auto &ws, uint16_t code, std::string_view reason) {
//             std::cout << "WebSocket closed with " << code
//                     << " reason: " << reason << "\n";
//         });

//     std::string upgrade_req =
//         "GET /chat HTTP/1.1\r\n"
//         "Host: localhost\r\n"
//         "Upgrade: websocket\r\n"
//         "Sec-WebSocket-Key: qwertyuiopasdfghjklzxcv\r\n"
//         "Sec-WebSocket-Version: 13\r\n\r\n";

//     std::string upgrade_resp = connect_request_response(upgrade_req, io);
//     EXPECT_NE(upgrade_resp.find("Switching Protocols"), std::string::npos)
//         << "WebSocket upgrade failed: " << upgrade_resp;
    
//     server.stop();
// }