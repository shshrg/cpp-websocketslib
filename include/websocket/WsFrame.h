#ifndef WEBSOCKETLIB_WSFRAME_H
#define WEBSOCKETLIB_WSFRAME_H

#include <cstdint>
#include <vector>
#include <string>

/**
 * @file wsframe.h
 * @brief WebSocket frame representation and serialization.
 *
 * This header defines the `WsFrame` struct, which models a single WebSocket frame
 * according to RFC 6455, including control bits, opcode, masking, payload length, and
 * the actual payload data. It also provides the `write_frame` function to serialize
 * a `WsFrame` into a byte vector suitable for sending over a network connection.
 *
 * Key features:
 * - Supports standard WebSocket frame fields: FIN, RSV1-3, opcode, MASK, and payload length.
 * - Handles extended payload lengths (126 and 127) automatically.
 * - Supports masking and unmasking of payload data as required by the protocol.
 *
 * Example usage:
 * @code
 * WsFrame frame;
 * frame.fin = true;
 * frame.opcode = WS_TEXT;
 * frame.payload_data = "Hello, WebSocket!";
 * frame.mask = false;
 *
 * auto bytes = write_frame(frame); // serialize frame for sending
 * @endcode
 */

/**
 * @brief Represents a single WebSocket frame.
 *
 * Contains control bits, opcode, masking info, payload length, and the actual payload.
 */
struct WsFrame {
    bool fin{};              ///< FIN bit: true if this is the final fragment
    bool rsv1{};             ///< Reserved bit 1 (for extensions)
    bool rsv2{};             ///< Reserved bit 2 (for extensions)
    bool rsv3{};             ///< Reserved bit 3 (for extensions)
    uint8_t opcode{};        ///< Frame opcode (WS_TEXT, WS_BINARY, etc.)
    bool mask{};             ///< Mask bit: true if payload is masked
    uint32_t masking_key{};  ///< Masking key for payload
    uint64_t payload_length{};///< Length of payload in bytes
    std::string payload_data; ///< Payload data as string
};

/**
 * @brief Serializes a WsFrame into a vector of bytes suitable for sending over the network.
 *
 * This function handles payload lengths, extended payload lengths, masking, and frame headers
 * according to the WebSocket protocol (RFC 6455).
 *
 * @param frame The WsFrame to serialize.
 * @return std::vector<uint8_t> Byte vector representing the WebSocket frame.
 */
inline std::vector<uint8_t> write_frame(const WsFrame &frame) {
    std::vector<uint8_t> bytes;

    const uint64_t payload_len = frame.payload_data.size();

    // Construct the first byte: FIN, RSV1-3, and opcode
    uint8_t byte0 = frame.fin << 7 |
                    frame.rsv1 << 6 |
                    frame.rsv2 << 5 |
                    frame.rsv3 << 4 |
                    frame.opcode & 0x0F;
    bytes.push_back(byte0);

    // Construct the second byte: MASK and payload length
    uint8_t byte1 = frame.mask << 7;

    if (payload_len <= 125) {
        byte1 |= static_cast<uint8_t>(payload_len);
        bytes.push_back(byte1);
    } else if (payload_len <= 0xFFFF) {
        byte1 |= 126;
        bytes.push_back(byte1);
        bytes.push_back((payload_len >> 8) & 0xFF);
        bytes.push_back(payload_len & 0xFF);
    } else {
        byte1 |= 127;
        bytes.push_back(byte1);
        for (int i = 7; i >= 0; --i) {
            bytes.push_back((payload_len >> (8 * i)) & 0xFF);
        }
    }

    // Append masking key if MASK is set
    if (frame.mask) {
        bytes.push_back((frame.masking_key >> 24) & 0xFF);
        bytes.push_back((frame.masking_key >> 16) & 0xFF);
        bytes.push_back((frame.masking_key >> 8) & 0xFF);
        bytes.push_back(frame.masking_key & 0xFF);
    }

    bytes.reserve(bytes.size() + payload_len);

    // Append payload data, applying mask if needed
    for (size_t i = 0; i < frame.payload_data.size(); ++i) {
        auto byte = static_cast<uint8_t>(frame.payload_data[i]);
        if (frame.mask) {
            byte ^= frame.masking_key >> ((3 - i % 4) * 8) & 0xFF;
        }
        bytes.push_back(byte);
    }

    return bytes;
}

#endif // WEBSOCKETLIB_WSFRAME_H
