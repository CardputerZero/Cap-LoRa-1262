#pragma once

#include <cstddef>

namespace cap_lora {

struct LoraInfo {
    int initialized = 0;
    int hw_ready = 0;
    int tx_mode = 0;
    int tx_in_progress = 0;
    int has_sent_message = 0;
    int rx_event = 0;
    int tx_event = 0;
    char spi_device[64] = {};
    char last_rx[128] = {};
    char last_tx[128] = {};
    char diag[256] = {};
    char probe_summary[256] = {};
    char probe_display[128] = {};
    char pi4io_status[160] = {};
    float rssi = 0.0f;
    float snr = 0.0f;
};

}  // namespace cap_lora
