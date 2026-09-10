// SDL-only Cap LoRa-1262 simulation. Hardware access lives in the sibling
// device translation unit, so the public facade does not need platform macros.

#include "lora/cap_lora_1262.hpp"

#include <cstdio>

namespace cap_lora {

CapLoRa1262::CapLoRa1262() = default;
CapLoRa1262::~CapLoRa1262() { shutdown(); }

bool CapLoRa1262::initialize()
{
    if (stop_requested_.load(std::memory_order_acquire)) return false;
    initialized_ = true;
    tx_in_progress_ = false;
    tx_mode_ = false;
    has_sent_message_ = false;
    rx_pending_ = false;
    tx_pending_ = false;
    queued_message_.clear();
    last_tx_.clear();
    last_rssi_ = -48.0f;
    last_snr_ = 7.0f;
    return true;
}

bool CapLoRa1262::probe() { return true; }
bool CapLoRa1262::set_rx_mode()
{
    if (!initialized_ || tx_in_progress_) return false;
    tx_mode_ = false;
    return true;
}
bool CapLoRa1262::set_tx_mode()
{
    if (!initialized_ || tx_in_progress_) return false;
    tx_mode_ = true;
    return true;
}

pw::Result<std::string> CapLoRa1262::msg_poll()
{
    if (!initialized_) return pw::Status::Unavailable();
    if (!rx_pending_ || queued_message_.empty()) return pw::Status::Unavailable();
    rx_pending_ = false;
    std::string message = queued_message_;
    queued_message_.clear();
    return message;
}

pw::Result<bool> CapLoRa1262::msg_send(const std::string& payload)
{
    if (payload.empty() || payload.size() > kMaxTextPayload) return pw::Status::InvalidArgument();
    if (!initialized_) return pw::Status::Unavailable();
    if (tx_in_progress_) return pw::Status::FailedPrecondition();
    tx_mode_ = false;
    has_sent_message_ = true;
    last_tx_ = payload;
    queued_message_ = "SDL echo: " + payload;
    rx_pending_ = true;
    tx_pending_ = true;
    last_rssi_ = -42.0f;
    last_snr_ = 9.5f;
    return true;
}

bool CapLoRa1262::send(const std::string& payload)
{
    return msg_send(payload).ok();
}

void CapLoRa1262::request_stop() noexcept { stop_requested_.store(true, std::memory_order_release); }
void CapLoRa1262::clear_stop() noexcept { stop_requested_.store(false, std::memory_order_release); }

void CapLoRa1262::shutdown()
{
    initialized_ = false;
    tx_in_progress_ = false;
    tx_mode_ = false;
    rx_pending_ = false;
    tx_pending_ = false;
    queued_message_.clear();
}

bool CapLoRa1262::setup() { return initialize(); }
bool CapLoRa1262::StartServer() { return initialize(); }
void CapLoRa1262::StopServer() { shutdown(); }
bool CapLoRa1262::isReady() const { return initialized_; }
bool CapLoRa1262::exec_send(const char* payload) { return payload != nullptr && send(payload); }
void CapLoRa1262::exec_poll() {}
void CapLoRa1262::start_receive() { if (initialized_) (void)set_rx_mode(); }
void CapLoRa1262::set_tx_mode(bool enabled) { if (enabled) (void)set_tx_mode(); else (void)set_rx_mode(); }

void CapLoRa1262::get_info(LoraInfo* info, bool drain_events) const
{
    if (!info) return;
    *info = LoraInfo{};
    std::snprintf(info->spi_device, sizeof(info->spi_device), "sdl://lora");
    std::snprintf(info->diag, sizeof(info->diag), "%s", initialized_ ? "SDL simulated LoRa ready" : "SDL simulated LoRa shutdown");
    std::snprintf(info->probe_summary, sizeof(info->probe_summary), "SDL simulated SPI/GPIO");
    std::snprintf(info->probe_display, sizeof(info->probe_display), "LoRa: SDL simulation");
    std::snprintf(info->pi4io_status, sizeof(info->pi4io_status), "SDL no-op PI4IO");
    info->initialized = initialized_ ? 1 : 0;
    info->hw_ready = initialized_ ? 1 : 0;
    info->tx_mode = tx_mode_ ? 1 : 0;
    info->tx_in_progress = tx_in_progress_ ? 1 : 0;
    info->has_sent_message = has_sent_message_ ? 1 : 0;
    info->rx_event = rx_pending_ ? 1 : 0;
    info->tx_event = tx_pending_ ? 1 : 0;
    if (!queued_message_.empty()) std::snprintf(info->last_rx, sizeof(info->last_rx), "%s", queued_message_.c_str());
    if (!last_tx_.empty()) std::snprintf(info->last_tx, sizeof(info->last_tx), "%s", last_tx_.c_str());
    info->rssi = last_rssi_;
    info->snr = last_snr_;
    if (drain_events) {
        rx_pending_ = false;
        tx_pending_ = false;
        queued_message_.clear();
    }
}

}  // namespace cap_lora
