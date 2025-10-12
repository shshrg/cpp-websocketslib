#include <cassert>
#include <iostream>

#include "http_request.h"
#include "http_response.h"

int main() {
    Request req;

    req.method = Method::POST;
    req.version = "HTTP/1.1";
    req.target = "/upload/?x=1&x=2";
    req.path = "/upload";
    req.body = "hello";


    req.headers.emplace("host", "example.com");
    req.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    req.headers.emplace("content-length", "5");
    req.headers.emplace("X-Foo", "a");
    req.headers.emplace("X-Foo", "b");

    req.query_params.emplace("x", "1");
    req.query_params.emplace("x", "2");


    assert(req.has_header("HOST")); // literal
    assert(req.has_header(std::string("Host")));

    assert(req.get_header_value("X-Foo", 1) == "a");

    assert(Request::method_name(req.method) == "POST");
    assert(req.has_content_type("text/plain"));

    assert(req.get_header_value("host") == "example.com");
    assert(req.get_header_value_count("x-foo") == 2);
    assert(req.get_header_value("missing").empty());

    auto cl = req.content_length();
    assert(cl.has_value() && cl.value() == 5);
    assert(req.content_type() == "text/plain; charset=utf-8");
    assert(req.has_content_type("text/plain"));

    assert(req.has_param("x"));
    assert(req.get_param_value_count("x") == 2);
    assert(req.get_param_value("x", 0) == "1");
    assert(req.get_param_value("x", 1) == "2");
    assert(req.get_param_value("x", 2).empty());
    assert(!req.has_param("y"));

    auto dumped = req.to_string();

    std::cout << dumped << std::endl;

    auto r404 = Response::not_found("No such page");
    auto s404 = r404.to_string();
    assert(s404.find("404") != std::string::npos);

    auto r400 = Response::bad_request("Malformed JSON");
    auto s400 = r400.to_string();
    assert(s400.find("400") != std::string::npos);
    assert(r400.content_type() == "application/json");


    auto rs = Response::serve_static(std::filesystem::path("publi"), "/readme.txt");
    auto ss = rs.to_string();
    std::cout << ss;
    std::cout << rs.sendfile_path << " " << rs.sendfile_size << std::endl;

    return 0;
}
