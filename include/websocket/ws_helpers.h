#ifndef WS_HELPERS
#define WS_HELPERS

#include <iostream>
#include <string>
#include <algorithm>


struct FrameBody {
    bool assembling = false;
    uint8_t opcode = 0;
    std::string msg;

    size_t max_size = 0;

    FrameBody(size_t max_sz) noexcept
        : max_size(max_sz) {
    }

    void set_max_size(size_t max_sz) noexcept {
        max_size = max_sz;
    }

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

    [[nodiscard]] bool would_exceed(size_t extra) const noexcept {
        return max_size != 0 && msg.size() + extra > max_size;
    }

    bool append(const std::string &bytes) {
        if (would_exceed(bytes.size()))
            return false;
        msg.append(bytes.data(), bytes.size());
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
