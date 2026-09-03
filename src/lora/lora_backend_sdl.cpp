#include "lora_backend.hpp"

#include <cstdio>
#include <cstring>
#include <mutex>

namespace cap_lora::backend {
namespace {

struct SdlRuntimeState {
    bool stop_requested = false;
    bool initialized = false;
    bool hw_ready = false;
    bool tx_mode = false;
    bool has_sent_message = false;
    bool rx_event = false;
    bool tx_event = false;
    float rssi = -48.0f;
    float snr = 7.0f;
    char last_rx[128] = "SDL simulated receive buffer";
    char last_tx[128] = "Hello from SDL LoRa";
    char diag[256] = "SDL simulated LoRa idle";
};

SdlRuntimeState g_state;
std::mutex g_mutex;

void copy_text(char* destination, size_t capacity, const char* source)
{
    if (!destination || capacity == 0) return;
    std::snprintf(destination, capacity, "%s", source ? source : "");
}

}  // namespace

bool initialize()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stop_requested) return false;
    g_state.initialized = true;
    g_state.hw_ready = true;
    std::snprintf(g_state.diag, sizeof(g_state.diag), "SDL simulated LoRa ready");
    return true;
}

void request_stop() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.stop_requested = true;
}

void clear_stop() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.stop_requested = false;
}

void poll()
{
}

void get_info(cap_lora::LoraInfo* info, bool drain_events)
{
    if (!info) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    *info = cap_lora::LoraInfo{};
    info->initialized = g_state.initialized ? 1 : 0;
    info->hw_ready = g_state.hw_ready ? 1 : 0;
    info->tx_mode = g_state.tx_mode ? 1 : 0;
    info->tx_in_progress = 0;
    info->has_sent_message = g_state.has_sent_message ? 1 : 0;
    info->rx_event = g_state.rx_event ? 1 : 0;
    info->tx_event = g_state.tx_event ? 1 : 0;
    copy_text(info->spi_device, sizeof(info->spi_device), "sdl://lora");
    copy_text(info->last_rx, sizeof(info->last_rx), g_state.last_rx);
    copy_text(info->last_tx, sizeof(info->last_tx), g_state.last_tx);
    copy_text(info->diag, sizeof(info->diag), g_state.diag);
    copy_text(info->probe_summary, sizeof(info->probe_summary), "SDL simulated SPI/GPIO");
    copy_text(info->probe_display, sizeof(info->probe_display), "LoRa: SDL simulation");
    copy_text(info->pi4io_status, sizeof(info->pi4io_status), "SDL no-op PI4IO");
    info->rssi = g_state.rssi;
    info->snr = g_state.snr;
    if (drain_events) {
        g_state.rx_event = false;
        g_state.tx_event = false;
    }
}

bool send_text(const char* payload)
{
    if (!payload) return false;
    const size_t payload_length = std::strlen(payload);
    if (payload_length == 0) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stop_requested || !g_state.initialized || !g_state.hw_ready) return false;
    if (payload_length > MAX_TEXT_PAYLOAD) {
        std::snprintf(g_state.diag, sizeof(g_state.diag),
                      "payload too long (%zu bytes; max %zu)", payload_length, MAX_TEXT_PAYLOAD);
        return false;
    }
    copy_text(g_state.last_tx, sizeof(g_state.last_tx), payload);
    std::snprintf(g_state.last_rx, sizeof(g_state.last_rx), "SDL echo: %.117s", g_state.last_tx);
    g_state.has_sent_message = true;
    g_state.tx_event = true;
    g_state.rx_event = true;
    g_state.rssi = -42.0f;
    g_state.snr = 9.5f;
    std::snprintf(g_state.diag, sizeof(g_state.diag), "SDL simulated packet sent");
    return true;
}

void start_receive()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stop_requested) return;
    g_state.tx_mode = false;
    std::snprintf(g_state.diag, sizeof(g_state.diag), "SDL simulated receive mode");
}

void set_tx_mode(bool enabled)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stop_requested) return;
    g_state.tx_mode = enabled;
    std::snprintf(g_state.diag, sizeof(g_state.diag), "%s",
                  enabled ? "SDL simulated TX mode" : "SDL simulated RX mode");
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.initialized = false;
    g_state.hw_ready = false;
    g_state.tx_mode = false;
    // Hardware shutdown clears edge/event latches while retaining message
    // history; mirror that lifecycle so a reopened page cannot replay stale
    // TX/RX notifications.
    g_state.rx_event = false;
    g_state.tx_event = false;
    std::snprintf(g_state.diag, sizeof(g_state.diag), "SDL simulated LoRa shutdown");
}

}  // namespace cap_lora::backend
