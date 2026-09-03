#pragma once

#include "lora_types.hpp"

#include <cstddef>

namespace cap_lora::backend {

// SX1262 text packets are retained in a fixed 128-byte buffer, including the
// terminator.  Reject larger API payloads instead of silently truncating them.
constexpr std::size_t MAX_TEXT_PAYLOAD = 127;

bool initialize();
void request_stop() noexcept;
void clear_stop() noexcept;
void poll();
void get_info(LoraInfo* info, bool drain_events);
bool send_text(const char* payload);
void start_receive();
void set_tx_mode(bool enabled);
void shutdown();

}  // namespace cap_lora::backend
