#include "gtest/gtest.h"
#include "server.h"
#include "config.h"
#include "websocket_client.h"

ServerConfig cfg;

void init_ws(WebSocketClient& client, const std::string& path) {
#ifdef USE_SSL
    if (cfg.use_ssl && !cfg.ssl_cert_file.empty())
        client.set_verify_cert_file(cfg.ssl_cert_file.string());
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
    WsInFrame f = client.read_frame();
    std::string resp(reinterpret_cast<char*>(f.payload.data()), f.payload.size());
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

    WsInFrame f1 = c1.read_frame();
    WsInFrame f2 = c2.read_frame();

    std::string r1(reinterpret_cast<char*>(f1.payload.data()), f1.payload.size());
    std::string r2(reinterpret_cast<char*>(f2.payload.data()), f2.payload.size());

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

TEST(WS_tests, Binary_Echo) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.WebSocketRouter("/binary")
        .on_message([](auto const& ws, std::string_view msg) {
            std::vector<uint8_t> bin(msg.begin(), msg.end());
            ws->send_binary_async(std::move(bin));
        });

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    WebSocketClient client(cfg.use_ssl);
    init_ws(client, "/binary");

    std::vector<uint8_t> sent_data = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
    client.send_binary(sent_data);

    WsInFrame f = client.read_frame();

    EXPECT_EQ(f.opcode, 0x2);
    EXPECT_EQ(f.payload, sent_data);

    server.stop();
    io.stop();
    thr.join();
}


TEST(WS_tests, Rapid_Fire_Queueing) {
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

    const int MSG_COUNT = 100;

    for (int i = 0; i < MSG_COUNT; ++i) {
        client.send_text("msg_" + std::to_string(i));
    }

    for (int i = 0; i < MSG_COUNT; ++i) {
        WsInFrame f = client.read_frame();
        EXPECT_EQ(f.opcode, 0x1);

        std::string expected = "msg_" + std::to_string(i);
        std::string actual(f.payload.begin(), f.payload.end());
        EXPECT_EQ(actual, expected);
    }

    server.stop();
    io.stop();
    thr.join();
}

TEST(WS_tests, Large_Payload_Echo) {
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

    std::string large_msg(2 * 1024 * 1024, 'X');

    client.send_text(large_msg);

    WsInFrame f = client.read_frame();

    EXPECT_EQ(f.opcode, 0x1);
    EXPECT_EQ(f.payload.size(), large_msg.size());

    server.stop();
    io.stop();
    thr.join();
}

TEST(WS_tests, Server_Initiated_Close) {
    asio::io_context io;
    auto address = asio::ip::make_address(cfg.address);
    Server server(io, address, cfg.port, cfg.use_ssl);

    server.WebSocketRouter("/closer")
        .on_open([](auto const& ws) {
            ws->close_async(4000, "Test Close");
        });

    server.start(cfg.threads);
    std::thread thr([&](){ io.run(); });

    WebSocketClient client(cfg.use_ssl);
    init_ws(client, "/closer");

    try {
        client.read_frame();
        FAIL() << "Should have received close frame";
    } catch (const std::exception& e) {
        std::string err = e.what();
        EXPECT_EQ(err, "Server closed connection");
    }

    server.stop();
    io.stop();
    thr.join();
}