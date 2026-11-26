#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace cap_lora {

inline constexpr int kCapMinKhz       = 868000;
inline constexpr int kCapMaxKhz       = 923000;
inline constexpr int kCapMaxTxDbm     = 22;
inline constexpr int kChatBandwidthHz = 125000;

struct RadioRegion {
    std::string_view code;
    int lower_khz;
    int upper_khz;
    int table_power_dbm;
    int duty_percent;
    int bandwidth_hz = kChatBandwidthHz;

    constexpr int usable_lower_khz() const
    {
        return lower_khz > kCapMinKhz ? lower_khz : kCapMinKhz;
    }
    constexpr int usable_upper_khz() const
    {
        return upper_khz < kCapMaxKhz ? upper_khz : kCapMaxKhz;
    }
    constexpr int frequency_khz() const
    {
        return (usable_lower_khz() + usable_upper_khz()) / 2;
    }
    constexpr float frequency_mhz() const
    {
        return frequency_khz() / 1000.0f;
    }
    constexpr float bandwidth_khz() const
    {
        return bandwidth_hz / 1000.0f;
    }
    constexpr int tx_power_dbm() const
    {
        return table_power_dbm < kCapMaxTxDbm ? table_power_dbm : kCapMaxTxDbm;
    }
    constexpr bool fits_cap() const
    {
        const int carrier_hz = frequency_khz() * 1000;
        return carrier_hz - bandwidth_hz / 2 >= usable_lower_khz() * 1000 &&
               carrier_hz + bandwidth_hz / 2 <= usable_upper_khz() * 1000;
    }
};

// Meshtastic LoRa Region table, in table order, limited to bands that can
// fit a complete signal in the Cap's specified RF range.
inline constexpr std::array kRadioRegions{
    RadioRegion{"US", 902000, 928000, 30, 100},
    RadioRegion{"EU_868", 869400, 869650, 27, 10},
    RadioRegion{"EU_N_868", 869400, 869650, 27, 10, 62500},
    RadioRegion{"JP", 920500, 923500, 13, 100},
    RadioRegion{"ANZ", 915000, 928000, 30, 100},
    RadioRegion{"KR", 920000, 923000, 23, 100},
    RadioRegion{"TW", 920000, 925000, 27, 100},
    RadioRegion{"RU", 868700, 869200, 20, 100},
    RadioRegion{"TH", 920000, 925000, 27, 10},
    RadioRegion{"MY_919", 919000, 924000, 27, 100},
    RadioRegion{"SG_923", 917000, 925000, 20, 100},
    RadioRegion{"BR_902", 902000, 907500, 30, 100},
    RadioRegion{"PH_868", 868000, 869400, 14, 100},
    RadioRegion{"PH_915", 915000, 918000, 24, 100},
};

static_assert(
    [] {
        for (const auto& region : kRadioRegions) {
            if (!region.fits_cap()) return false;
        }
        return true;
    }(),
    "Every preset must fit inside both the Cap and the regional band");

struct RadioSettings {
    std::size_t region = 0;

    constexpr bool complete() const
    {
        return region > 0 && region <= kRadioRegions.size();
    }
    constexpr const RadioRegion* preset() const
    {
        return complete() ? &kRadioRegions[region - 1] : nullptr;
    }
};

constexpr RadioSettings radio_settings_for_code(std::string_view code)
{
    for (std::size_t i = 0; i < kRadioRegions.size(); ++i) {
        if (kRadioRegions[i].code == code) return {i + 1};
    }
    return {};
}

constexpr RadioSettings step_radio_region(RadioSettings settings, bool forward)
{
    const auto size = kRadioRegions.size();
    if (!settings.complete()) return {forward ? 1 : size};
    const auto step = forward ? 1 : size - 1;
    return {(settings.region - 1 + step) % size + 1};
}

}  // namespace cap_lora
