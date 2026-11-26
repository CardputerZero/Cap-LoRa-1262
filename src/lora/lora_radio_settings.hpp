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
    int duty_permille;
    int bandwidth_hz         = kChatBandwidthHz;
    int router_duty_permille = 0;

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

// Meshtastic LoRa Region table through NP_865, in table order. Duty is
// stored in tenths of a percent to also represent EU_866's 2.5% limit.
inline constexpr std::array kRadioRegions{
    RadioRegion{"US", 902000, 928000, 30, 1000},
    RadioRegion{"EU_433", 433000, 434000, 10, 100},
    RadioRegion{"EU_868", 869400, 869650, 27, 100},
    RadioRegion{"EU_866", 865600, 867600, 27, 25, 125000, 100},
    RadioRegion{"EU_N_868", 869400, 869650, 27, 100, 62500},
    RadioRegion{"CN", 470000, 510000, 19, 1000},
    RadioRegion{"JP", 920500, 923500, 13, 1000},
    RadioRegion{"ANZ", 915000, 928000, 30, 1000},
    RadioRegion{"ANZ_433", 433050, 434790, 14, 1000},
    RadioRegion{"KR", 920000, 923000, 23, 1000},
    RadioRegion{"TW", 920000, 925000, 27, 1000},
    RadioRegion{"RU", 868700, 869200, 20, 1000},
    RadioRegion{"IN", 865000, 867000, 30, 1000},
    RadioRegion{"NZ_865", 864000, 868000, 36, 1000},
    RadioRegion{"TH", 920000, 925000, 27, 100},
    RadioRegion{"UA_433", 433000, 434700, 10, 100},
    RadioRegion{"MY_433", 433000, 435000, 20, 1000},
    RadioRegion{"MY_919", 919000, 924000, 27, 1000},
    RadioRegion{"SG_923", 917000, 925000, 20, 1000},
    RadioRegion{"KZ_433", 433075, 434775, 10, 1000},
    RadioRegion{"KZ_863", 863000, 868000, 30, 1000},
    RadioRegion{"BR_902", 902000, 907500, 30, 1000},
    RadioRegion{"PH_433", 433000, 434700, 10, 1000},
    RadioRegion{"PH_868", 868000, 869400, 14, 1000},
    RadioRegion{"PH_915", 915000, 918000, 24, 1000},
    RadioRegion{"NP_865", 865000, 868000, 30, 1000},
};

struct RadioSettings {
    std::size_t region = 0;

    constexpr const RadioRegion* entry() const
    {
        return region > 0 && region <= kRadioRegions.size() ? &kRadioRegions[region - 1] : nullptr;
    }
    constexpr const RadioRegion* preset() const
    {
        const auto* choice = entry();
        return choice && choice->fits_cap() ? choice : nullptr;
    }
    constexpr bool complete() const
    {
        return preset() != nullptr;
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
    if (!settings.entry()) return {forward ? 1 : size};
    const auto step = forward ? 1 : size - 1;
    return {(settings.region - 1 + step) % size + 1};
}

}  // namespace cap_lora
