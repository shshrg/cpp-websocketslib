#include "gtest/gtest.h"
#include "server.h"
#include "config.h"
#include "websocket_client.h"

ServerConfig cfg;

void init_ws(WebSocketClient& client, const std::string& path) {
#ifdef USE_SSL
    if (cfg.use_ssl && !cfg.ssl_cert_file.empty())
        client.set_verify_cert_file(cfg.ssl_cert_file);
#endif

    std::string host = cfg.address.empty() ? "127.0.0.1" : cfg.address;
    std::string port = std::to_string(cfg.port);

    client.connect(host, port, path);
}

TEST(WS_tests, Handshake_Success) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.WebSocketRouter("/chat")
        .on_open([](auto const& ws){})
        .on_message([](auto const& ws, std::string_view){})
        .on_close([](auto const& ws, uint16_t, std::string_view){});

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    EXPECT_NO_THROW({
        WebSocketClient client(cfg.use_ssl);
        init_ws(client, "/chat");
    });

    server.stop();
    io.stop();
    thr.join();
}

TEST(WS_tests, Echo_Message) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.WebSocketRouter("/echo")
        .on_message([](auto const& ws, std::string_view msg) {
            ws->send_text_async(std::string(msg));
        });

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    WebSocketClient client(cfg.use_ssl);
    init_ws(client, "/echo");

    client.send_text("ping");
    std::string resp = client.read_frame_text();
    EXPECT_EQ(resp, "ping");

    server.stop();
    io.stop();
    thr.join();
}

TEST(WS_tests, Two_Clients_Broadcast) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    std::vector<std::shared_ptr<WebSocket>> conns;
    std::mutex mtx;

    server.WebSocketRouter("/chat")
        .on_open([&](auto const& ws){
            std::lock_guard lg(mtx);
            conns.push_back(ws);
        })
        .on_message([&](auto const& ws, std::string_view msg){
            std::lock_guard lg(mtx);
            for (auto& c : conns) c->send_text_async(std::string(msg));
        });

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    WebSocketClient c1(cfg.use_ssl);
    WebSocketClient c2(cfg.use_ssl);

    init_ws(c1, "/chat");
    init_ws(c2, "/chat");

    c1.send_text("hello");

    std::string r1 = c1.read_frame_text();
    std::string r2 = c2.read_frame_text();

    EXPECT_EQ(r1, "hello");
    EXPECT_EQ(r2, "hello");

    server.stop();
    io.stop();
    thr.join();
}

TEST(WS_tests, Invalid_Route_Handshake_Fails) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    EXPECT_THROW({
        WebSocketClient client(cfg.use_ssl);
        init_ws(client, "/notfound");
    }, std::exception);

    server.stop();
    io.stop();
    thr.join();
}
