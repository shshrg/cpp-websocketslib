#ifndef WS_HELPERS
#define WS_HELPERS

#include <iostream>
#include <string>
#include <algorithm>

using WsOpenHandler = std::function<void()>;
using WsMessageHandler = std::function<void(std::string_view msg)>;
using WsCloseHandler = std::function<void(uint16_t code, std::string_view reason)>;

struct WsHandlers {
    WsOpenHandler on_open{};
    WsMessageHandler on_message{};
    WsCloseHandler on_close{};
};

struct FrameBody {
    bool assembling = false;
    uint8_t opcode = 0;
    std::string msg;

    void reset() noexcept {
        assembling = false;
        opcode = 0;
        msg.clear();
    }

    void start(uint8_t op, size_t reserve_hint = 0) {
        assembling = true;
        opcode = op;
        msg.clear();
        if (reserve_hint)
            msg.reserve(reserve_hint);
    }

    bool append(const std::string &bytes) {
        msg.append(bytes.data(), bytes.size());
        return true;
    }

    bool append_masked(const uint8_t *data, size_t len, uint32_t mask_key) {
        const size_t old_size = msg.size();
        msg.resize(old_size + len);
        for (size_t i = 0; i < len; ++i) {
            auto m = static_cast<uint8_t>(
                (mask_key >> ((3 - (i & 3)) * 8)) & 0xFF
            );
            msg[old_size + i] = static_cast<char>(data[i] ^ m);
        }
        return true;
    }


    [[nodiscard]] size_t size() const noexcept { return msg.size(); }
    [[nodiscard]] bool empty() const noexcept { return msg.empty(); }
};



enum : uint8_t {
    WS_CONT = 0x0,
    WS_TEXT = 0x1,
    WS_BINARY = 0x2,
    WS_CLOSE = 0x8,
    WS_PING = 0x9,
    WS_PONG = 0xA
};

inline std::string build_close_payload(uint16_t code, std::string_view reason_utf8) {
    std::string out;
    out.resize(2);
    out[0] = static_cast<char>((code >> 8) & 0xFF);
    out[1] = static_cast<char>(code & 0xFF);
    const size_t room = 125 - 2; // 123
    if (!reason_utf8.empty()) {
        const size_t take = std::min(room, reason_utf8.size());
        out.append(reason_utf8.data(), take);
    }
    return out;
}

#endif //WS_HELPERS
