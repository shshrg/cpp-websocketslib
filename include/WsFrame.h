#ifndef WEBSOCKETLIB_WSFRAME_H
#define WEBSOCKETLIB_WSFRAME_H
#include <vector>
#include <string>
#include <optional>

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

inline size_t ws_next_frame_size(const std::vector<uint8_t>& buf) {
    if (buf.size() < 2) return 0;
    size_t pos = 2;
    uint8_t b1 = buf[1];
    uint64_t len7 = (b1 & 0x7F);
    size_t ext = 0;
    if (len7 == 126) ext = 2;
    else if (len7 == 127) ext = 8;

    if (buf.size() < pos + ext) return 0;
    uint64_t payload_len = len7;
    if (ext == 2) {
        payload_len = (static_cast<uint64_t>(buf[pos])<<8) | buf[pos+1];
    } else if (ext == 8) {
        payload_len = 0;
        for (int i=0;i<8;++i) payload_len = (payload_len<<8) | buf[pos+i];
    }
    pos += ext;

    bool mask = (b1 & 0x80) != 0;
    if (mask) {
        if (buf.size() < pos + 4) return 0;
        pos += 4;
    }
    if (buf.size() < pos + payload_len) return 0;
    return pos + payload_len;
}


inline std::optional<WsFrame> parse_frame(std::vector<uint8_t> bytes) {
    if (bytes.size() < 2)
        return std::nullopt;

    WsFrame frame;

    frame.fin = (bytes[0] & 0x80) != 0;
    frame.rsv1 = (bytes[0] & 0x40) != 0;
    frame.rsv2 = (bytes[0] & 0x20) != 0;
    frame.rsv3 = (bytes[0] & 0x10) != 0;
    frame.opcode = bytes[0] & 0x0F;
    frame.mask = (bytes[1] & 0x80) != 0;
    frame.payload_length = (bytes[1] & 0x7F);

    int offset = 0;
    if (frame.payload_length == 0x7F) offset = 8; //127
    if (frame.payload_length == 0x7E) offset = 2; //126

    uint64_t extended_length = 0;
    for (int i = 0; (i < offset) && (i < 6); i++) {
        extended_length = (extended_length << 8) | bytes[2 + i];
    }
    if (offset != 0)
        frame.payload_length = extended_length;


    if (frame.mask) {
        frame.masking_key = (bytes[offset + 2] << 24) |
                            (bytes[offset + 3] << 16) |
                            (bytes[offset + 4] << 8) |
                            bytes[offset + 5];
        offset += 4;
    }

    frame.payload_data.resize(frame.payload_length);
    for (size_t i = 0; i < frame.payload_length; i++) {
        uint8_t byte = bytes[2 + offset + i];
        if (frame.mask) {
            byte ^= ((frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
        }
        frame.payload_data[i] = byte;
    }

    return frame;
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
