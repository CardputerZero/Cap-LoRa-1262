// Smoke test for the Pigweed subset compiled into device builds
// (pw_spi_linux + pw_i2c_linux plus their facade backends).
//
// Only paths that never touch hardware are exercised: object construction,
// pure address/status logic, and the "bus node missing" error path of the
// I2C initiator. This guards the include wiring and symbol closure of
// cap_lora_pigweed without requiring a CardputerZero.

#include "pw_i2c/address.h"
#include "pw_i2c_linux/initiator.h"
#include "pw_digital_io_linux/digital_io.h"
#include "pw_spi_linux/spi.h"
#include "pw_status/status.h"

#include "test_support.hpp"

int main()
{
    // pw_status: the status vocabulary the bus drivers report. A default
    // constructed Status and OkStatus() both denote success.
    CHECK(pw::OkStatus() == pw::Status());
    CHECK(pw::OkStatus().ok());
    CHECK(!pw::Status::InvalidArgument().ok());
    CHECK(pw::Status::InvalidArgument() != pw::OkStatus());

    // pw_spi_linux: the chip selector is a no-op on spidev, and the
    // initiator only stores the file descriptor until a transfer. A negative
    // fd is safe: the destructor skips close() for it.
    pw::spi::LinuxChipSelector chip_selector;
    CHECK(chip_selector.SetActive(true) == pw::OkStatus());
    CHECK(chip_selector.SetActive(false) == pw::OkStatus());
    {
        pw::spi::LinuxInitiator initiator(-1, 1000000);
    }

    // pw_digital_io_linux: the chip facade is lightweight and does not open a
    // descriptor until a line is requested.
    pw::digital_io::LinuxDigitalIoChip gpio_chip(-1);
    gpio_chip.Close();

    // pw_i2c: address factory logic (implemented in pw_i2c/address.cc).
    const pw::i2c::Address seven_bit = pw::i2c::Address::SevenBit(0x20);
    CHECK(seven_bit.GetAddress() == 0x20);
    CHECK(!seven_bit.IsTenBit());
    constexpr pw::i2c::Address k_compile_time = pw::i2c::Address::SevenBit<0x21>();
    CHECK(k_compile_time.GetAddress() == 0x21);
    constexpr pw::i2c::Address k_ten_bit = pw::i2c::Address::TenBit<0x155>();
    static_assert(k_ten_bit.IsTenBit());

    // pw_i2c_linux: opening a nonexistent bus node must fail cleanly with
    // InvalidArgument instead of crashing or leaking a file descriptor.
    pw::Result<int> bus = pw::i2c::LinuxInitiator::OpenI2cBus("/nonexistent-cap-lora-i2c-bus");
    CHECK(!bus.ok());
    CHECK(bus.status() == pw::Status::InvalidArgument());

    return 0;
}
