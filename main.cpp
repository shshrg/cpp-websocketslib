#include "httplib.h"
#include <iostream>
 

// very basic httplib server with some examples

int main() {
  httplib::Server svr;
 
  svr.Get("/", [](const httplib::Request& /*req*/, httplib::Response& res) {
    res.set_redirect("/hello");
  });
 
  svr.Get("/hello", [](const httplib::Request& /*req*/, httplib::Response& res) {
    res.set_content("<h1>Hello, cpp-httplib!</h1>", "text/html");
  });
 
  svr.Get("/users/:id", [](const httplib::Request& req, httplib::Response& res) {
    auto user_id = req.path_params.at("id");
    res.set_content("User ID: " + user_id, "text/plain");
  });
 
  svr.Post("/submit", [](const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("name")) {
      auto name = req.get_param_value("name");
      res.set_content("Hello, " + name + "!", "text/plain");
    } else {
      res.status = 400;
      res.set_content("Missing 'name' parameter", "text/plain");
    }
  });
 
  if (!svr.set_mount_point("/assets", "./public")) {
    std::cerr << "Failed to set mount point: directory doesn't exist\n";
    return 1;
  }
 
  svr.set_error_handler([](const httplib::Request& /*req*/, httplib::Response& res) {
    res.set_content("<h1>Error " + std::to_string(res.status) + "</h1>", "text/html");
  });
 
  svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    std::cout << req.method << " " << req.path << " - " << res.status << std::endl;
  });
 
  std::cout << "Server starting on http://localhost:8080" << std::endl;
  svr.listen("localhost", 8080);
 
  return 0;
}