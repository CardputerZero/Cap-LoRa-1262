#include "lora/cap_lora_1262.hpp"
#include "test_support.hpp"

#include <cstring>
#include <string>

int main()
{
    cap_lora::CapLoRa1262 radio;
    cap_lora::LoraInfo info{};
    const auto settings = cap_lora::radio_settings_for_code("US");
    CHECK(!radio.exec_send("before init"));
    CHECK(!radio.initialize({}));
    CHECK(!radio.initialize({cap_lora::kRadioRegions.size() + 1}));
    CHECK(!radio.initialize(cap_lora::radio_settings_for_code("CN")));
    CHECK(!radio.exec_send("incomplete configuration"));
    CHECK(radio.initialize(settings));
    radio.get_info(&info, false);
    CHECK(info.initialized && info.hw_ready);
    CHECK(info.frequency_mhz == 912.5f && info.output_power_dbm == 22 && info.bandwidth_khz == 125.0f);
    CHECK(std::strcmp(info.spi_device, "sdl://lora") == 0);
    CHECK(std::strcmp(info.probe_display, "LoRa: SDL simulation") == 0);

    CHECK(radio.exec_send("test payload"));
    radio.get_info(&info, false);
    CHECK(info.has_sent_message && info.rx_event && info.tx_event);
    CHECK(std::strcmp(info.last_rx, "test payload") == 0);
    CHECK(info.rssi == -42.0f && info.snr == 9.5f);

    const std::string maximum(cap_lora::MAX_TEXT_PAYLOAD, 'M');
    CHECK(radio.exec_send(maximum.c_str()));
    radio.get_info(&info, false);
    CHECK(std::string(info.last_rx) == maximum);

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
    CHECK(!radio.initialize(settings));
    radio.clear_stop();
    const auto eu = cap_lora::radio_settings_for_code("EU_868");
    const auto narrow = cap_lora::radio_settings_for_code("EU_N_868");
    CHECK(radio.initialize(eu));
    CHECK(radio.initialize(narrow));
    radio.get_info(&info, false);
    CHECK(info.frequency_mhz == 869.525f && info.output_power_dbm == 22 && info.bandwidth_khz == 62.5f);
    std::size_t frames = 0;
    while (frames < 100 && radio.msg_send(maximum).ok()) ++frames;
    CHECK(frames > 0 && frames < 100);
    CHECK(radio.msg_send(maximum).status().IsResourceExhausted());
    CHECK(radio.initialize(narrow));
    CHECK(radio.msg_send(maximum).status().IsResourceExhausted());
    CHECK(radio.initialize(eu));
    CHECK(radio.msg_send(maximum).ok());
    CHECK(radio.msg_send(maximum).status().IsResourceExhausted());

    CHECK(radio.initialize(cap_lora::radio_settings_for_code("JP")));
    radio.get_info(&info, false);
    CHECK(info.frequency_mhz == 921.75f && info.output_power_dbm == 13);
    CHECK(radio.exec_send("unlimited region"));
    radio.shutdown();
    return 0;
}
