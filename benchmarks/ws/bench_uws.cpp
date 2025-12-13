#include <App.h>
#include <iostream>
#include <string>

struct UserData {};

int main() {
    using namespace std;

    unsigned short port = 9001;

    uWS::App()
        .ws<UserData>("/ws-echo", {
            .open = [](auto *ws) {
                cout << "[uWS] ws open\n";
            },
            .message = [](auto *ws,
                          std::string_view msg,
                          uWS::OpCode opCode) {
                ws->send(msg, opCode);
            },
            .close = [](auto *ws, int code, std::string_view message) {
                cout << "[uWS] ws close: " << code
                     << " reason=" << message << "\n";
            }
        })
        .listen(port, [port](auto *listenSocket) {
            if (listenSocket) {
                std::cout << "[uWS] WS echo listening on ws://127.0.0.1:"
                          << port << "/ws-echo\n";
            } else {
                std::cout << "[uWS] Failed to listen on port " << port << "\n";
            }
        })
        .run();

    std::cout << "[uWS] stopped\n";
    return 0;
}
