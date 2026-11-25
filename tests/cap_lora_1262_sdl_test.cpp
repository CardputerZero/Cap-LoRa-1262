#include "lora/cap_lora_1262.hpp"
#include "test_support.hpp"

#include <cstring>
#include <string>

int main()
{
    cap_lora::CapLoRa1262 radio;
    cap_lora::LoraInfo info{};
    const cap_lora::RadioSettings settings{915200, 0};
    CHECK(!radio.exec_send("before init"));
    CHECK(!radio.initialize({}));
    CHECK(!radio.initialize({settings.frequency_khz}));
    CHECK(!radio.initialize({0, settings.tx_power_dbm}));
    CHECK(!radio.initialize({923001, 22}));
    CHECK(!radio.initialize({868000, 23}));
    CHECK(!radio.initialize({915250, 0}));
    CHECK(!radio.initialize({915200, 12}));
    CHECK(!radio.exec_send("incomplete configuration"));
    CHECK(radio.initialize(settings));
    radio.get_info(&info, false);
    CHECK(info.initialized && info.hw_ready);
    CHECK(info.frequency_mhz == settings.frequency_mhz() && info.output_power_dbm == 0);
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
    CHECK(radio.initialize({868100, -9}));
    CHECK(radio.initialize({922500, 14}));
    radio.get_info(&info, false);
    CHECK(info.frequency_mhz == 922.5f && info.output_power_dbm == 14);
    radio.shutdown();

    const auto &frequencies = cap_lora::kRadioFrequenciesKhz;
    const auto &powers = cap_lora::kRadioPowersDbm;
    CHECK(cap_lora::step_radio_preset(frequencies, 0, true) == frequencies.front());
    CHECK(cap_lora::step_radio_preset(frequencies, 0, false) == frequencies.back());
    CHECK(cap_lora::step_radio_preset(frequencies, frequencies.back(), true) == frequencies.front());
    CHECK(cap_lora::step_radio_preset(frequencies, frequencies.front(), false) == frequencies.back());
    CHECK(cap_lora::step_radio_preset(powers, -9, true) == 0);
    CHECK(cap_lora::step_radio_preset(powers, 0, false) == -9);
    return 0;
}
