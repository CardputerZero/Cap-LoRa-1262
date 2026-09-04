#include "lora/lora_backend.hpp"
#include "test_support.hpp"

#include <cstring>
#include <string>

int main()
{
    cap_lora::LoraInfo info{};
    CHECK(!cap_lora::backend::send_text("before init"));
    CHECK(cap_lora::backend::initialize());
    cap_lora::backend::get_info(&info, false);
    CHECK(info.initialized && info.hw_ready);
    CHECK(std::strcmp(info.spi_device, "sdl://lora") == 0);
    CHECK(std::strcmp(info.probe_display, "LoRa: SDL simulation") == 0);

    CHECK(cap_lora::backend::send_text("test payload"));
    cap_lora::backend::get_info(&info, false);
    CHECK(info.has_sent_message && info.rx_event && info.tx_event);
    CHECK(std::strcmp(info.last_tx, "test payload") == 0);
    CHECK(std::strcmp(info.last_rx, "SDL echo: test payload") == 0);
    CHECK(info.rssi == -42.0f && info.snr == 9.5f);

    const std::string max_payload(cap_lora::backend::MAX_TEXT_PAYLOAD, 'A');
    CHECK(cap_lora::backend::send_text(max_payload.c_str()));
    const std::string oversized(cap_lora::backend::MAX_TEXT_PAYLOAD + 1, 'B');
    CHECK(!cap_lora::backend::send_text(oversized.c_str()));
    cap_lora::backend::get_info(&info, false);
    CHECK(std::strstr(info.diag, "payload too long") != nullptr);

    cap_lora::backend::get_info(&info, true);
    CHECK(info.rx_event && info.tx_event);
    cap_lora::backend::get_info(&info, false);
    CHECK(!info.rx_event && !info.tx_event);

    cap_lora::backend::start_receive();
    cap_lora::backend::get_info(&info, false);
    CHECK(!info.tx_mode);
    cap_lora::backend::set_tx_mode(true);
    cap_lora::backend::get_info(&info, false);
    CHECK(info.tx_mode);
    cap_lora::backend::shutdown();
    cap_lora::backend::get_info(&info, false);
    CHECK(!info.initialized && !info.hw_ready);

    // Stop requests must prevent new work until explicitly cleared, matching
    // the hardware backend's shutdown lifecycle.
    cap_lora::backend::request_stop();
    CHECK(!cap_lora::backend::initialize());
    CHECK(!cap_lora::backend::send_text("after stop"));
    cap_lora::backend::clear_stop();
    CHECK(cap_lora::backend::initialize());
    cap_lora::backend::get_info(&info, false);
    CHECK(!info.rx_event && !info.tx_event);
    cap_lora::backend::shutdown();
    return 0;
}
