#ifndef WEBSOCKETLIB_WSFRAME_H
#define WEBSOCKETLIB_WSFRAME_H
#include <cstdint>
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