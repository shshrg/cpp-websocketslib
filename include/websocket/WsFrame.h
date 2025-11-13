#ifndef WEBSOCKETLIB_WSFRAME_H
#define WEBSOCKETLIB_WSFRAME_H
#include <vector>
#include <string>

struct WsFrame {
    bool fin{};
    bool rsv1{};
    bool rsv2{};
    bool rsv3{};
    uint8_t opcode{};
    bool mask{};
    uint32_t masking_key{};
    uint64_t payload_length{};
    std::string payload_data;
};

enum class HeaderParseResult {
    NeedMore,
    Error,
    Ok
};

inline HeaderParseResult ws_try_parse_header(const std::vector<uint8_t> &buf,
                                             size_t start,
                                             WsFrame &frame, size_t &header_bytes) {
    const size_t available = buf.size() - start;
    if (available < 2) {
        return HeaderParseResult::NeedMore;
    }

    const uint8_t *p = buf.data() + start;
    const uint8_t b0 = p[0];
    const uint8_t b1 = p[1];

    WsFrame f{};
    f.fin = (b0 & 0x80) != 0;
    f.rsv1 = (b0 & 0x40) != 0;
    f.rsv2 = (b0 & 0x20) != 0;
    f.rsv3 = (b0 & 0x10) != 0;
    f.opcode = (b0 & 0x0F);
    f.mask = (b1 & 0x80) != 0;

    uint64_t len7 = (b1 & 0x7F);
    size_t off = 2;
    uint64_t payload_len = len7;

    if (len7 == 126) {
        if (available < off + 2) return HeaderParseResult::NeedMore;
        payload_len = (static_cast<uint64_t>(p[off]) << 8) |
                      static_cast<uint64_t>(p[off + 1]);
        off += 2;
    } else if (len7 == 127) {
        if (available < off + 8) return HeaderParseResult::NeedMore;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | p[off + i];
        }
        off += 8;
    }

    if (f.mask) {
        if (available < off + 4) return HeaderParseResult::NeedMore;
        f.masking_key = p[off] << 24 |
                        p[off + 1] << 16 |
                        p[off + 2] << 8 |
                        p[off + 3];
        off += 4;
    }

    f.payload_length = payload_len;
    header_bytes = off;

    frame = f;
    return HeaderParseResult::Ok;
}


inline std::vector<uint8_t> write_frame(const WsFrame &frame) {
    std::vector<uint8_t> bytes;

    uint8_t byte0 = (frame.fin << 7) |
                    (frame.rsv1 << 6) |
                    (frame.rsv2 << 5) |
                    (frame.rsv3 << 4) |
                    (frame.opcode & 0x0F);
    bytes.push_back(byte0);

    uint8_t byte1 = (frame.mask << 7);

    if (frame.payload_length <= 125) {
        byte1 |= static_cast<uint8_t>(frame.payload_length);
        bytes.push_back(byte1);
    } else if (frame.payload_length <= 0xFFFF) {
        byte1 |= 126;
        bytes.push_back(byte1);
        bytes.push_back((frame.payload_length >> 8) & 0xFF);
        bytes.push_back(frame.payload_length & 0xFF);
    } else {
        byte1 |= 127;
        bytes.push_back(byte1);
        for (int i = 7; i >= 0; --i) {
            bytes.push_back((frame.payload_length >> (8 * i)) & 0xFF);
        }
    }

    if (frame.mask) {
        bytes.push_back((frame.masking_key >> 24) & 0xFF);
        bytes.push_back((frame.masking_key >> 16) & 0xFF);
        bytes.push_back((frame.masking_key >> 8) & 0xFF);
        bytes.push_back(frame.masking_key & 0xFF);
    }

    for (size_t i = 0; i < frame.payload_data.size(); ++i) {
        auto byte = static_cast<uint8_t>(frame.payload_data[i]);
        if (frame.mask) {
            byte ^= (frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF;
        }
        bytes.push_back(byte);
    }

    return bytes;
}

#endif //WEBSOCKETLIB_WSFRAME_H
