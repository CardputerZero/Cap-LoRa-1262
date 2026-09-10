/**
 * @file pi4ioe5v6408_driver.cpp
 * @author Forairaaaaa, lovyan03
 * @brief PI4IOE5V6408 8-bit I/O expander driver
 * @version 0.3
 * @date 2025-06-11
 *
 * @copyright Copyright (c) 2024
 *
 * Adapted for Cap-LoRa-1262: register access now goes through Pigweed's
 * pw::i2c::Initiator instead of M5Unified's I2C_Class. Register map and
 * bit semantics follow the datasheet
 * (https://www.diodes.com/assets/Datasheets/PI4IOE5V6408.pdf) and are
 * unchanged from the M5Unified original:
 *   0x01 output port / ID, 0x02 polarity, 0x03 direction (1 = output),
 *   0x05 output value, 0x07 high-impedance, 0x0B pull up/down enable,
 *   0x0D pull selection (1 = up), 0x0F input, 0x11 interrupt mask,
 *   0x13 interrupt status (read to reset).
 */
#include "PI4IOE5V6408_Class.hpp"

#include <cstddef>

namespace m5
{

PI4IOE5V6408_Class::PI4IOE5V6408_Class(pw::i2c::Initiator& initiator, std::uint8_t i2c_addr,
                                       pw::chrono::SystemClock::duration timeout)
    : initiator_(&initiator), address_(pw::i2c::Address::SevenBit(i2c_addr)), timeout_(timeout)
{
}

PI4IOE5V6408_Class::PI4IOE5V6408_Class(std::shared_ptr<pw::i2c::LinuxInitiator> initiator,
                                       std::uint8_t i2c_addr, pw::chrono::SystemClock::duration timeout)
    : owned_initiator_(std::move(initiator)),
      initiator_(owned_initiator_.get()),
      address_(pw::i2c::Address::SevenBit(i2c_addr)),
      timeout_(timeout)
{
}

bool PI4IOE5V6408_Class::begin()
{
    if (!initiator_) return false;
    const pw::Result<std::uint8_t> id = readRegister8(0x01);
    return id.ok() && *id != 0;
}

// --- register-level access --------------------------------------------------

bool PI4IOE5V6408_Class::writeRegister8(std::uint8_t reg, std::uint8_t value)
{
    // One atomic write transaction: register address followed by the value.
    const std::byte payload[2] = {static_cast<std::byte>(reg), static_cast<std::byte>(value)};
    if (!initiator_) return false;
    last_status_               = initiator_->WriteFor(address_, payload, timeout_);
    return last_status_.ok();
}

pw::Result<std::uint8_t> PI4IOE5V6408_Class::readRegister8(std::uint8_t reg)
{
    // Write the register address, then read the value back in the same
    // transaction (repeated start), so no other bus client can move the
    // register pointer in between.
    std::byte reg_address = static_cast<std::byte>(reg);
    std::byte value       = static_cast<std::byte>(0);
    if (!initiator_) return pw::Status::Unavailable();
    last_status_          = initiator_->WriteReadFor(address_,
                                           pw::span<const std::byte>(&reg_address, 1),
                                           pw::span<std::byte>(&value, 1), timeout_);
    if (!last_status_.ok())
    {
        return last_status_;
    }
    return static_cast<std::uint8_t>(value);
}

bool PI4IOE5V6408_Class::bitOn(std::uint8_t reg, std::uint8_t bit)
{
    const pw::Result<std::uint8_t> current = readRegister8(reg);
    if (!current.ok())
    {
        return false;
    }
    return writeRegister8(reg, static_cast<std::uint8_t>(*current | bit));
}

bool PI4IOE5V6408_Class::bitOff(std::uint8_t reg, std::uint8_t bit)
{
    const pw::Result<std::uint8_t> current = readRegister8(reg);
    if (!current.ok())
    {
        return false;
    }
    return writeRegister8(reg, static_cast<std::uint8_t>(*current & ~bit));
}

// --- pin-level API ----------------------------------------------------------

// false input, true output
bool PI4IOE5V6408_Class::setDirection(uint8_t pin, bool direction)
{
    if (pin >= 8)
    {
        return false;
    }
    if (direction)
    {
        return bitOn(0x03, 1 << pin); // Output, set 1
    }
    return bitOff(0x03, 1 << pin); // Input, set 0
}

bool PI4IOE5V6408_Class::setPullMode(uint8_t pin, gpio_pull_t mode)
{
    if (pin >= 8)
        return false;
    const auto bit = 1 << pin;
    switch (mode)
    {
    case pull_none:
        return bitOff(0x0B, bit);
    case pull_up:
        if (!bitOn(0x0D, bit))
        {
            return false;
        }
        return bitOn(0x0B, bit);
    case pull_down:
        if (!bitOff(0x0D, bit))
        {
            return false;
        }
        return bitOn(0x0B, bit);
    default:
        return false;
    }
}

bool PI4IOE5V6408_Class::setHighImpedance(uint8_t pin, bool enable)
{
    if (pin >= 8)
    {
        return false;
    }
    if (enable)
    {
        return bitOn(0x07, 1 << pin);
    }
    return bitOff(0x07, 1 << pin);
}

bool PI4IOE5V6408_Class::getWriteValue(uint8_t pin)
{
    auto data = readRegister8(0x05);
    return data.ok() && (*data & (1 << pin)) != 0;
}

bool PI4IOE5V6408_Class::digitalWrite(uint8_t pin, bool level)
{
    if (pin >= 8)
    {
        return false;
    }
    if (level)
    {
        return bitOn(0x05, 1 << pin);
    }
    return bitOff(0x05, 1 << pin);
}

bool PI4IOE5V6408_Class::digitalRead(uint8_t pin)
{
    auto data = readRegister8(0x0F);
    return data.ok() && (*data & (1 << pin)) != 0;
}

bool PI4IOE5V6408_Class::resetIrq()
{
    // Reading the interrupt status register clears it; the value itself is
    // not needed here.
    const pw::Result<std::uint8_t> status = readRegister8(0x13);
    return status.ok();
}

bool PI4IOE5V6408_Class::disableIrq()
{
    return writeRegister8(0x11, 0xFF);
}

bool PI4IOE5V6408_Class::enableIrq()
{
    return writeRegister8(0x11, 0x00);
}

} // namespace m5
