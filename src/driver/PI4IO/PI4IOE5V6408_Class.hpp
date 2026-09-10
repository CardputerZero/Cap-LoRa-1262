/**
 * @file PI4IOE5V6408_Class.hpp
 * @author Forairaaaaa, lovyan03
 * @brief PI4IOE5V6408 8-bit I/O expander driver
 * @version 0.3
 * @date 2025-06-11
 *
 * @copyright Copyright (c) 2024
 *
 * Adapted for Cap-LoRa-1262: the M5Unified I2C_Class transport was replaced
 * by Pigweed's pw::i2c::Initiator (dependencies/pigweed), so the driver runs
 * on Linux through pw_i2c_linux without M5Unified. Register semantics and
 * the pin-level API are unchanged from the M5Unified original.
 */
#ifndef __M5_PI4IOE5V6408_H__
#define __M5_PI4IOE5V6408_H__

#include <chrono>
#include <cstdint>

#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_i2c_linux/initiator.h"
#include <memory>
namespace m5
{
// https://www.diodes.com/assets/Datasheets/PI4IOE5V6408.pdf
class PI4IOE5V6408_Class
{
public:
    static constexpr std::uint8_t DEFAULT_ADDRESS = 0x43;

    // Pull-mode vocabulary carried over from M5Unified's IOExpander API so
    // existing call sites keep compiling without m5unified_common.h.
    enum gpio_pull_t
    {
        pull_none = 0,
        pull_up,
        pull_down,
    };

    // The initiator is not owned: the caller keeps the bus session (and its
    // exclusive lock) alive for the lifetime of this object.
    explicit PI4IOE5V6408_Class(pw::i2c::Initiator& initiator,
                                std::uint8_t i2c_addr = DEFAULT_ADDRESS,
                                pw::chrono::SystemClock::duration timeout = std::chrono::milliseconds(100));
    explicit PI4IOE5V6408_Class(std::shared_ptr<pw::i2c::LinuxInitiator> initiator,
                                std::uint8_t i2c_addr = DEFAULT_ADDRESS,
                                pw::chrono::SystemClock::duration timeout = std::chrono::milliseconds(100));

    // Returns true when the device ID register (0x01) reads back non-zero,
    // i.e. the expander acknowledges on the bus.
    bool begin();

    // --- register-level access ------------------------------------------------
    // Used by the runtime controller for snapshot/restore; the pin-level API
    // below is built on the same primitives.

    bool writeRegister8(std::uint8_t reg, std::uint8_t value);

    pw::Result<std::uint8_t> readRegister8(std::uint8_t reg);

    // Read-modify-write helpers returning false when either half fails.
    bool bitOn(std::uint8_t reg, std::uint8_t bit);
    bool bitOff(std::uint8_t reg, std::uint8_t bit);

    // Status of the most recent bus transaction; lets callers report a
    // meaningful code instead of a bare bool.
    pw::Status lastStatus() const { return last_status_; }

    // --- pin-level API (IOExpander semantics, unchanged) ----------------------

    // false input, true output
    bool setDirection(uint8_t pin, bool direction);

    bool setPullMode(uint8_t pin, gpio_pull_t mode);

    bool setHighImpedance(uint8_t pin, bool enable);

    bool getWriteValue(uint8_t pin);

    bool digitalWrite(uint8_t pin, bool level);

    bool digitalRead(uint8_t pin);

    // Reading the interrupt status register (0x13) also resets it.
    bool resetIrq();

    // Mask (disable) / unmask (enable) interrupts on all pins.
    bool disableIrq();

    bool enableIrq();

    bool prob()
    {
        // Probe the device's fixed default address without touching any
        // register state. Keep the status so callers can inspect the reason
        // for a failed probe through lastStatus().
        last_status_ = initiator_->ProbeDeviceFor(pw::i2c::Address::SevenBit(DEFAULT_ADDRESS), timeout_);
        return last_status_.ok();
    }

private:
    std::shared_ptr<pw::i2c::LinuxInitiator> owned_initiator_;
    pw::i2c::Initiator* initiator_;
    pw::i2c::Address address_;
    pw::chrono::SystemClock::duration timeout_;
    pw::Status last_status_ = pw::OkStatus();
};

} // namespace m5

#endif
