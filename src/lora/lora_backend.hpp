#pragma once

#include "lora_types.hpp"

namespace cap_lora::backend {

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
