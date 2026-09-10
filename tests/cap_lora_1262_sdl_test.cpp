#include "lora/cap_lora_1262.hpp"
#include "test_support.hpp"

#include <cstring>
#include <string>

int main()
{
    cap_lora::CapLoRa1262 radio;
    cap_lora::LoraInfo info{};
    CHECK(!radio.exec_send("before init"));
    CHECK(radio.initialize());
    radio.get_info(&info, false);
    CHECK(info.initialized && info.hw_ready);
    CHECK(std::strcmp(info.spi_device, "sdl://lora") == 0);
    CHECK(std::strcmp(info.probe_display, "LoRa: SDL simulation") == 0);

    CHECK(radio.exec_send("test payload"));
    radio.get_info(&info, false);
    CHECK(info.has_sent_message && info.rx_event && info.tx_event);
    CHECK(std::strcmp(info.last_rx, "SDL echo: test payload") == 0);
    CHECK(info.rssi == -42.0f && info.snr == 9.5f);

    const std::string oversized(cap_lora::MAX_TEXT_PAYLOAD + 1, 'X');
    CHECK(!radio.exec_send(oversized.c_str()));
    radio.get_info(&info, true);
    CHECK(info.rx_event && info.tx_event);
    radio.get_info(&info, false);
    CHECK(!info.rx_event && !info.tx_event);

    radio.shutdown();
    radio.get_info(&info, false);
    CHECK(!info.initialized && !info.hw_ready);
    radio.request_stop();
    CHECK(!radio.initialize());
    radio.clear_stop();
    CHECK(radio.initialize());
    radio.shutdown();
    return 0;
}
