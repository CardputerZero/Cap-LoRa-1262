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

struct DecodedPayload {
    std::string message;
    std::string nickname;
};

inline bool is_printable_ascii(std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return character >= 0x20 && character <= 0x7e;
    });
}

inline std::string encode(std::string_view message, std::string_view nickname)
{
    if (message.empty() || message.size() > kMaxMessageBytes) return {};
    if (nickname.empty() || nickname.size() > kMaxNicknameBytes || !is_printable_ascii(nickname) ||
        message.size() + nickname.size() + 1 > kRadioPayloadBytes)
        return std::string(message);

    constexpr uint8_t kNicknameMarker = 0x80U;
    std::string payload;
    payload.reserve(1 + nickname.size() + message.size());
    payload.push_back(static_cast<char>(kNicknameMarker | static_cast<uint8_t>(nickname.size())));
    payload.append(nickname);
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
                return {std::string(payload.substr(1 + nickname_length)), std::string(nickname)};
            }
        }
    }
    return {std::string(payload), {}};
}

}  // namespace lora_chat_protocol
