#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <string>
#include "http/request.h"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using namespace asio::experimental::awaitable_operators;

awaitable<void> do_read(tcp::socket& socket)
{
    try
    {
        std::vector<char> buf(1024);
        while (true)
        {
            std::size_t n = co_await socket.async_read_some(asio::buffer(buf), asio::use_awaitable);
            std::string data(buf.data(), n);
            std::cout << "Server replied: " << data << "\n";
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Read stopped: " << e.what() << "\n";
    }
}

awaitable<void> do_write(tcp::socket& socket)
{
    try
    {
        while (true)
        {
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) break;

            Request request;
            request.method = Method::POST;
            request.version = "HTTP/1.1";
            request.target = "/test?x=1&x=2";

            request.headers.emplace("Host", "localhost");
            request.headers.emplace("Content-Length", std::to_string(line.size()));
            request.headers.emplace("Content-Type", "text/plain; charset=utf-8");

            request.body = line;
            std::string req_str = request.to_string();
            // std::cout << "Sending request:\n" << req_str << std::endl;
            co_await asio::async_write(socket, asio::buffer(req_str), asio::use_awaitable);
        }

        socket.close();
    }
    catch (std::exception& e)
    {
        std::cout << "Write stopped: " << e.what() << "\n";
    }
}

int main()
{
    try
    {
        asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "12345");
        tcp::socket socket(io_context);
        asio::connect(socket, endpoints);
        std::cout << "Connected to server.\n";

        co_spawn(io_context, do_read(socket), detached);
        co_spawn(io_context, do_write(socket), detached);

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}