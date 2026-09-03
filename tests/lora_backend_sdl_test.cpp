#include "lora/lora_backend.hpp"

#include <cassert>
#include <cstring>
#include <string>

int main()
{
    cap_lora::LoraInfo info{};
    assert(!cap_lora::backend::send_text("before init"));
    assert(cap_lora::backend::initialize());
    cap_lora::backend::get_info(&info, false);
    assert(info.initialized && info.hw_ready);
    assert(std::strcmp(info.spi_device, "sdl://lora") == 0);
    assert(std::strcmp(info.probe_display, "LoRa: SDL simulation") == 0);

    assert(cap_lora::backend::send_text("test payload"));
    cap_lora::backend::get_info(&info, false);
    assert(info.has_sent_message && info.rx_event && info.tx_event);
    assert(std::strcmp(info.last_tx, "test payload") == 0);
    assert(std::strcmp(info.last_rx, "SDL echo: test payload") == 0);
    assert(info.rssi == -42.0f && info.snr == 9.5f);

    const std::string max_payload(cap_lora::backend::MAX_TEXT_PAYLOAD, 'A');
    assert(cap_lora::backend::send_text(max_payload.c_str()));
    const std::string oversized(cap_lora::backend::MAX_TEXT_PAYLOAD + 1, 'B');
    assert(!cap_lora::backend::send_text(oversized.c_str()));
    cap_lora::backend::get_info(&info, false);
    assert(std::strstr(info.diag, "payload too long") != nullptr);

    cap_lora::backend::get_info(&info, true);
    assert(info.rx_event && info.tx_event);
    cap_lora::backend::get_info(&info, false);
    assert(!info.rx_event && !info.tx_event);

    cap_lora::backend::start_receive();
    cap_lora::backend::get_info(&info, false);
    assert(!info.tx_mode);
    cap_lora::backend::set_tx_mode(true);
    cap_lora::backend::get_info(&info, false);
    assert(info.tx_mode);
    cap_lora::backend::shutdown();
    cap_lora::backend::get_info(&info, false);
    assert(!info.initialized && !info.hw_ready);

    // Stop requests must prevent new work until explicitly cleared, matching
    // the hardware backend's shutdown lifecycle.
    cap_lora::backend::request_stop();
    assert(!cap_lora::backend::initialize());
    assert(!cap_lora::backend::send_text("after stop"));
    cap_lora::backend::clear_stop();
    assert(cap_lora::backend::initialize());
    cap_lora::backend::get_info(&info, false);
    assert(!info.rx_event && !info.tx_event);
    cap_lora::backend::shutdown();
    return 0;
}
