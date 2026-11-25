#pragma once

#include <array>

namespace cap_lora {

// 868 MHz is the previous P2P channel. The others are selected 125 kHz
// channel centers from LoRa Alliance RP002 within the Cap's 868-923 MHz range.
inline constexpr std::array kRadioFrequenciesKhz{
    868000, 868100, 868300, 868500, 868900, 869100,  // Legacy, EU868, RU864
    902300, 903900, 914900, 915200,                  // US915, AU915
    916600, 916800, 917300, 917500, 921400, 921600,  // AS923-3, AS923-4, AS923-2
    922100, 922300, 922500,                          // KR920
};

// Conducted output power at the radio, in discrete steps within the SX1262 range.
inline constexpr std::array kRadioPowersDbm{-9, 0, 5, 10, 11, 13, 14, 17, 20, 22};
inline constexpr int kUnsetTxPowerDbm = -10;

template <std::size_t N>
constexpr std::size_t radio_preset_index(const std::array<int, N>& presets, int value)
{
    for (std::size_t i = 0; i < N; ++i) {
        if (presets[i] == value) return i;
    }
    return N;
}

template <std::size_t N>
constexpr int step_radio_preset(const std::array<int, N>& presets, int value, bool forward)
{
    const auto i = radio_preset_index(presets, value);
    if (i == N) return forward ? presets.front() : presets.back();
    return presets[(i + (forward ? 1 : N - 1)) % N];
}

struct RadioSettings {
    int frequency_khz = 0;
    int tx_power_dbm  = kUnsetTxPowerDbm;

    constexpr bool valid_frequency() const
    {
        return radio_preset_index(kRadioFrequenciesKhz, frequency_khz) < kRadioFrequenciesKhz.size();
    }

    constexpr bool valid_power() const
    {
        return radio_preset_index(kRadioPowersDbm, tx_power_dbm) < kRadioPowersDbm.size();
    }

    constexpr bool complete() const
    {
        return valid_frequency() && valid_power();
    }
    constexpr float frequency_mhz() const
    {
        return frequency_khz / 1000.0f;
    }
};

}  // namespace cap_lora
