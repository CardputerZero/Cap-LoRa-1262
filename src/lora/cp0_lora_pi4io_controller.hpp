#pragma once

namespace cp0_lora_pi4io_controller {

bool scan_and_initialize();
void request_stop() noexcept;
void clear_stop() noexcept;
void shutdown();
const char *status();

} // namespace cp0_lora_pi4io_controller
