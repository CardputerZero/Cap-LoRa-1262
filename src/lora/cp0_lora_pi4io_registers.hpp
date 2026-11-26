#pragma once

#include "driver/PI4IO/PI4IOE5V6408_Class.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cp0_lora_pi4io_controller {

inline constexpr uint8_t kAntennaPin = 0x01;

struct AntennaRegister {
    uint8_t address;
    uint8_t enabled;
    const char* name;
};

// Configure the latch before enabling the output driver. The PI4IOE5V6408
// uses 0x01 for device control and reserves 0x02.
inline constexpr std::array kAntennaRegisters{
    AntennaRegister{0x05, kAntennaPin, "OUT"},
    AntennaRegister{0x03, kAntennaPin, "DIR"},
    AntennaRegister{0x07, 0, "HIZ"},
};
using AntennaSnapshot = std::array<uint8_t, kAntennaRegisters.size()>;

constexpr uint8_t with_antenna_bit(uint8_t current, uint8_t value)
{
    return static_cast<uint8_t>((current & ~kAntennaPin) | (value & kAntennaPin));
}

inline pw::Result<AntennaSnapshot> read_antenna_snapshot(m5::PI4IOE5V6408_Class& pi4io)
{
    AntennaSnapshot snapshot{};
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        const auto value = pi4io.readRegister8(kAntennaRegisters[i].address);
        if (!value.ok()) return value.status();
        snapshot[i] = *value;
    }
    return snapshot;
}

template <typename ShouldStop>
pw::Status enable_antenna(m5::PI4IOE5V6408_Class& pi4io, const AntennaSnapshot& snapshot, ShouldStop should_stop,
                          const char*& failed_register)
{
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        if (should_stop()) return pw::Status::Cancelled();
        const auto& reg    = kAntennaRegisters[i];
        const auto enabled = with_antenna_bit(snapshot[i], reg.enabled);
        if (enabled != snapshot[i] && !pi4io.writeRegister8(reg.address, enabled)) {
            failed_register = reg.name;
            return pi4io.lastStatus();
        }
    }
    return pw::OkStatus();
}

inline bool restore_antenna(m5::PI4IOE5V6408_Class& pi4io, const AntennaSnapshot& snapshot)
{
    bool ok = true;
    // Restore drive state before the old latch value, preserving any
    // unrelated pins that another client changed during this session.
    for (std::size_t i = snapshot.size(); i > 0;) {
        --i;
        const auto address = kAntennaRegisters[i].address;
        const auto current = pi4io.readRegister8(address);
        if (!current.ok()) {
            ok = false;
            continue;
        }
        const auto restored = with_antenna_bit(*current, snapshot[i]);
        if (restored != *current && !pi4io.writeRegister8(address, restored)) ok = false;
    }
    return ok;
}

}  // namespace cp0_lora_pi4io_controller
