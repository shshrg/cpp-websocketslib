#ifndef WS_HELPERS
#define WS_HELPERS

#include <iostream>
#include <string>
#include <algorithm>

/**
 * @file ws_helpers.h
 * @brief Helper structures and utilities for WebSocket frame handling.
 *
 * This header provides:
 * - `FrameBody`: a helper structure to assemble fragmented WebSocket messages
 *   (text or binary) and manage size limits.
 * - WebSocket opcodes constants (WS_CONT, WS_TEXT, WS_BINARY, etc.).
 * - `build_close_payload()`: utility function to construct a WebSocket close
 *   frame payload with a code and optional UTF-8 reason.
 *
 * These utilities simplify handling of fragmented messages, control frames,
 * and protocol-compliant close frames.
 */

/**
 * @brief Helper structure to assemble fragmented WebSocket frames.
 *
 * Manages accumulation of message fragments for text or binary frames.
 */
struct FrameBody {
    bool assembling = false;  ///< True if currently assembling a fragmented message
    uint8_t opcode = 0;       ///< Opcode of the current message (WS_TEXT, WS_BINARY)
    std::string msg;          ///< Accumulated payload

    size_t max_size = 0;      ///< Maximum allowed size for the message

    /**
     * @brief Constructs a FrameBody with a maximum size.
     * @param max_sz Maximum allowed size for the message
     */
    FrameBody(size_t max_sz) noexcept
        : max_size(max_sz) {
    }

    /**
     * @brief Sets the maximum allowed size.
     * @param max_sz Maximum size
     */
    void set_max_size(size_t max_sz) noexcept {
        max_size = max_sz;
    }

    /**
     * @brief Resets the frame body state, clearing any accumulated data.
     */
    void reset() noexcept {
        assembling = false;
        opcode = 0;
        msg.clear();
    }

    /**
     * @brief Starts a new message assembly.
     * @param op Opcode of the message (WS_TEXT or WS_BINARY)
     * @param reserve_hint Optional hint to reserve space for performance
     */
    void start(uint8_t op, size_t reserve_hint = 0) {
        assembling = true;
        opcode = op;
        msg.clear();
        if (reserve_hint)
            msg.reserve(reserve_hint);
    }

    /**
     * @brief Checks if adding extra bytes would exceed max_size.
     * @param extra Number of bytes to append
     * @return true if it would exceed max_size
     */
    [[nodiscard]] bool would_exceed(size_t extra) const noexcept {
        return max_size != 0 && msg.size() + extra > max_size;
    }

    /**
     * @brief Appends bytes to the message if within max_size.
     * @param bytes Data to append
     * @return true if append succeeded, false if it would exceed max_size
     */
    bool append(const std::string &bytes) {
        if (would_exceed(bytes.size()))
            return false;
        msg.append(bytes.data(), bytes.size());
        return true;
    }

    /**
     * @brief Returns the current size of the message.
     */
    [[nodiscard]] size_t size() const noexcept { return msg.size(); }

    /**
     * @brief Returns true if the message is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return msg.empty(); }
};

/**
 * @brief WebSocket frame opcodes.
 */
enum : uint8_t {
    WS_CONT = 0x0,   ///< Continuation frame
    WS_TEXT = 0x1,   ///< Text frame
    WS_BINARY = 0x2, ///< Binary frame
    WS_CLOSE = 0x8,  ///< Close frame
    WS_PING = 0x9,   ///< Ping frame
    WS_PONG = 0xA    ///< Pong frame
};

/**
 * @brief Builds a payload for a WebSocket close frame.
 * @param code Close code (e.g., 1000 for normal closure)
 * @param reason_utf8 Optional UTF-8 encoded close reason
 * @return Encoded string suitable as close frame payload (max 125 bytes)
 */
inline std::string build_close_payload(uint16_t code, std::string_view reason_utf8) {
    std::string out;
    out.resize(2);
    out[0] = static_cast<char>((code >> 8) & 0xFF);
    out[1] = static_cast<char>(code & 0xFF);
    const size_t room = 125 - 2; // Maximum payload size minus code
    if (!reason_utf8.empty()) {
        const size_t take = std::min(room, reason_utf8.size());
        out.append(reason_utf8.data(), take);
    }
    return out;
}

#endif // WS_HELPERS
