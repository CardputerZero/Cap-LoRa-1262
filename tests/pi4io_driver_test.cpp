// Protocol test for the PI4IOE5V6408 driver on top of pw::i2c.
//
// A fake pw::i2c::Initiator records every transaction and emulates the
// register map, so the register-level and pin-level API can be verified
// down to the exact bytes on the (emulated) wire, with no hardware.

#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

#include "driver/PI4IO/PI4IOE5V6408_Class.hpp"

namespace {

struct Transaction {
    uint8_t address = 0;
    bool is_write = false;
    std::vector<uint8_t> bytes; // written bytes, or [reg, returned] for reads

    bool operator==(const Transaction &other) const
    {
        return address == other.address && is_write == other.is_write && bytes == other.bytes;
    }
};

class RecordingInitiator : public pw::i2c::Initiator {
public:
    RecordingInitiator() : pw::i2c::Initiator(pw::i2c::Initiator::Feature::kStandard) {}

    pw::Status DoWriteReadFor(pw::i2c::Address address,
                              pw::ConstByteSpan tx_buffer,
                              pw::ByteSpan rx_buffer,
                              pw::chrono::SystemClock::duration) override
    {
        if (fail_next) {
            fail_next = false;
            return pw::Status::Unavailable();
        }
        Transaction transaction;
        transaction.address = address.GetAddress();
        transaction.is_write = rx_buffer.empty();
        for (const std::byte byte : tx_buffer)
            transaction.bytes.push_back(std::to_integer<uint8_t>(byte));
        if (transaction.is_write && tx_buffer.size() == 2) {
            registers[transaction.bytes[0]] = transaction.bytes[1];
        } else if (!transaction.is_write) {
            // Register read: serve the value and record it.
            CHECK(rx_buffer.size() == 1);
            rx_buffer[0] = static_cast<std::byte>(registers[transaction.bytes[0]]);
            transaction.bytes.push_back(std::to_integer<uint8_t>(rx_buffer[0]));
        }
        transactions.push_back(transaction);
        return pw::OkStatus();
    }

    std::vector<Transaction> transactions;
    uint8_t registers[256] = {};
    bool fail_next = false;
};

Transaction write_of(uint8_t reg, uint8_t value)
{
    return Transaction{0x43, true, {reg, value}};
}

Transaction read_of(uint8_t reg, uint8_t value)
{
    return Transaction{0x43, false, {reg, value}};
}

} // namespace

int main()
{
    using m5::PI4IOE5V6408_Class;

    {
        // Register writes are one atomic write transaction [reg, value].
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        CHECK(pi4io.writeRegister8(0x11, 0xFF));
        CHECK(bus.transactions.size() == 1);
        CHECK(bus.transactions[0] == write_of(0x11, 0xFF));
        CHECK(bus.registers[0x11] == 0xFF);

        // Register reads are a write-read transaction; the returned value
        // comes back through pw::Result.
        bus.registers[0x05] = 0xA5;
        const pw::Result<uint8_t> value = pi4io.readRegister8(0x05);
        CHECK(value.ok());
        CHECK(*value == 0xA5);
        CHECK(bus.transactions.size() == 2);
        CHECK(bus.transactions[1] == read_of(0x05, 0xA5));

        // bitOn/bitOff are read-modify-write pairs.
        bus.registers[0x03] = 0x0F;
        CHECK(pi4io.bitOn(0x03, 0x30));
        CHECK(bus.registers[0x03] == 0x3F);
        CHECK(pi4io.bitOff(0x03, 0x0F));
        CHECK(bus.registers[0x03] == 0x30);
        CHECK(bus.transactions.size() == 6); // 2 per read-modify-write

        // A failing transaction leaves lastStatus() set for diagnostics.
        bus.fail_next = true;
        CHECK(!pi4io.writeRegister8(0x05, 0x00));
        CHECK(pi4io.lastStatus() == pw::Status::Unavailable());
    }

    {
        // begin(): a non-zero ID register means the device answered.
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        bus.registers[0x01] = 0x20;
        CHECK(pi4io.begin());
        CHECK(bus.transactions.size() == 1);
        CHECK(bus.transactions[0] == read_of(0x01, 0x20));

        bus.registers[0x01] = 0x00;
        CHECK(!pi4io.begin()); // ID reads zero

        bus.fail_next = true;
        CHECK(!pi4io.begin()); // transfer failed
    }

    {
        // Direction: output sets the bit in 0x03, input clears it.
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        bus.registers[0x03] = 0x00;
        CHECK(pi4io.setDirection(4, true));
        CHECK(bus.registers[0x03] == 0x10);
        CHECK(pi4io.setDirection(4, false));
        CHECK(bus.registers[0x03] == 0x00);
        CHECK(!pi4io.setDirection(8, true)); // only 8 pins
        CHECK(bus.registers[0x03] == 0x00);
    }

    {
        // Pull modes: pull-up selects up (0x0D) and enables (0x0B);
        // pull-down selects down and enables; none disables.
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        CHECK(pi4io.setPullMode(2, PI4IOE5V6408_Class::pull_up));
        CHECK(bus.registers[0x0D] == 0x04);
        CHECK(bus.registers[0x0B] == 0x04);
        CHECK(pi4io.setPullMode(2, PI4IOE5V6408_Class::pull_down));
        CHECK(bus.registers[0x0D] == 0x00);
        CHECK(bus.registers[0x0B] == 0x04);
        CHECK(pi4io.setPullMode(2, PI4IOE5V6408_Class::pull_none));
        CHECK(bus.registers[0x0B] == 0x00);
        CHECK(!pi4io.setPullMode(8, PI4IOE5V6408_Class::pull_up));
    }

    {
        // High impedance (0x07), output values (0x05) and inputs (0x0F).
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        CHECK(pi4io.setHighImpedance(1, true));
        CHECK(bus.registers[0x07] == 0x02);
        CHECK(pi4io.setHighImpedance(1, false));
        CHECK(bus.registers[0x07] == 0x00);

        CHECK(pi4io.digitalWrite(7, true));
        CHECK(bus.registers[0x05] == 0x80);
        CHECK(pi4io.getWriteValue(7));
        CHECK(pi4io.digitalWrite(7, false));
        CHECK(bus.registers[0x05] == 0x00);
        CHECK(!pi4io.getWriteValue(7));
        CHECK(!pi4io.digitalWrite(8, true));

        bus.registers[0x0F] = 0x40;
        CHECK(pi4io.digitalRead(6));
        CHECK(!pi4io.digitalRead(5));
    }

    {
        // Interrupts: 0x11 masks all pins when disabled, unmasks when
        // enabled; reading the status register 0x13 resets it.
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus);
        CHECK(pi4io.disableIrq());
        CHECK(bus.registers[0x11] == 0xFF);
        CHECK(pi4io.enableIrq());
        CHECK(bus.registers[0x11] == 0x00);
        CHECK(pi4io.resetIrq());
        CHECK(bus.transactions.back() == read_of(0x13, 0x00));
    }

    {
        // A custom address is used for every transaction.
        RecordingInitiator bus;
        PI4IOE5V6408_Class pi4io(bus, 0x21);
        CHECK(pi4io.writeRegister8(0x05, 0x11));
        CHECK(bus.transactions.size() == 1);
        CHECK(bus.transactions[0].address == 0x21);
    }

    return 0;
}
