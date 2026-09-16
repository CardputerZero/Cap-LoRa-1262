#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace lora_chat_protocol {

inline constexpr std::size_t kRadioPayloadBytes = 127;
inline constexpr std::size_t kMaxNicknameBytes  = 10;
inline constexpr std::size_t kMaxMessageBytes   = kRadioPayloadBytes - kMaxNicknameBytes - 1;
inline constexpr std::size_t kReplyReferenceBytes = 9;
inline constexpr std::size_t kMaxReplyMessageBytes = kMaxMessageBytes - kReplyReferenceBytes;

struct DecodedPayload {
    std::string message;
    std::string nickname;
    uint32_t reply_id = 0;
    bool has_reply = false;
};

inline uint32_t message_id(std::string_view message)
{
    uint32_t value = 2166136261U;
    for (const unsigned char character : message) {
        value ^= character;
        value *= 16777619U;
    }
    return value;
}

inline bool is_printable_ascii(std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return character >= 0x20 && character <= 0x7e;
    });
}

inline std::string encode(std::string_view message, std::string_view nickname, std::string_view reply_to = {})
{
    const std::size_t message_limit = reply_to.empty() ? kMaxMessageBytes : kMaxReplyMessageBytes;
    if (message.empty() || message.size() > message_limit) return {};
    if (nickname.empty() || nickname.size() > kMaxNicknameBytes || !is_printable_ascii(nickname) ||
        message.size() + nickname.size() + 1 + (reply_to.empty() ? 0 : kReplyReferenceBytes) > kRadioPayloadBytes)
        return std::string(message);

    constexpr uint8_t kNicknameMarker = 0x80U;
    std::string payload;
    payload.reserve(1 + nickname.size() + message.size() + (reply_to.empty() ? 0 : kReplyReferenceBytes));
    payload.push_back(static_cast<char>(kNicknameMarker | static_cast<uint8_t>(nickname.size())));
    payload.append(nickname);
    if (!reply_to.empty()) {
        constexpr char kHexDigits[] = "0123456789ABCDEF";
        const uint32_t reply_id = message_id(reply_to);
        payload.push_back('\x1d');
        for (int shift = 28; shift >= 0; shift -= 4) payload.push_back(kHexDigits[(reply_id >> shift) & 0x0fU]);
    }
    payload.append(message);
    return payload;
}

inline DecodedPayload decode(std::string_view payload)
{
    constexpr uint8_t kNicknameMarker     = 0x80U;
    constexpr uint8_t kNicknameLengthMask = 0x7FU;
    if (!payload.empty()) {
        const uint8_t marker               = static_cast<uint8_t>(payload.front());
        const std::size_t nickname_length = marker & kNicknameLengthMask;
        if ((marker & kNicknameMarker) != 0 && nickname_length > 0 && nickname_length <= kMaxNicknameBytes &&
            payload.size() > 1 + nickname_length) {
            const std::string_view nickname = payload.substr(1, nickname_length);
            if (is_printable_ascii(nickname)) {
                std::string_view message = payload.substr(1 + nickname_length);
                uint32_t reply_id = 0;
                bool has_reply = message.size() > kReplyReferenceBytes && message.front() == '\x1d';
                for (std::size_t index = 1; has_reply && index < kReplyReferenceBytes; ++index) {
                    const char digit = message[index];
                    if (digit >= '0' && digit <= '9')
                        reply_id = (reply_id << 4U) | static_cast<uint32_t>(digit - '0');
                    else if (digit >= 'A' && digit <= 'F')
                        reply_id = (reply_id << 4U) | static_cast<uint32_t>(digit - 'A' + 10);
                    else
                        has_reply = false;
                }
                if (has_reply) message.remove_prefix(kReplyReferenceBytes);
                return {std::string(message), std::string(nickname), reply_id, has_reply};
            }
        }
    }
    return {std::string(payload), {}};
}

}  // namespace lora_chat_protocol
