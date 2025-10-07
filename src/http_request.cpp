#include "http_request.h"
#include <sstream>

std::string Request::to_string() const {
    std::ostringstream ss;


    ss << method << " " << target << " " << version << '\n';

    for (const auto& header : headers) {
        ss << header.first << ": " << header.second << std::endl;
    }

    if (!body.empty() && headers.find("Content-Length") != headers.end()) {
        ss << "Content-Length: " << body.size() << std::endl;
    }

    ss << "\n";

    ss << body;

    return ss.str();

}
