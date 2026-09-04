#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include "test_support.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace {

uint8_t registers_by_address[256] = {};
uint8_t selected_register = 0;
int open_count = 0;
int close_count = 0;
int data_write_count = 0;
int fail_data_write = 0;

int fake_open(const char *path, int, ...)
{
    CHECK(std::strcmp(path, "/dev/i2c-1") == 0);
    ++open_count;
    return 42;
}

int fake_close(int fd)
{
    CHECK(fd == 42);
    ++close_count;
    return 0;
}

int fake_ioctl(int fd, unsigned long request, ...)
{
    CHECK(fd == 42);
    CHECK(request == I2C_TIMEOUT || request == I2C_RETRIES || request == I2C_SLAVE);
    return 0;
}

int fake_flock(int fd, int operation)
{
    CHECK(fd == 42);
    CHECK(operation == (LOCK_EX | LOCK_NB));
    return 0;
}

ssize_t fake_write(int fd, const void *buffer, size_t size)
{
    CHECK(fd == 42);
    const auto *bytes = static_cast<const uint8_t *>(buffer);
    if (size == 1) {
        selected_register = bytes[0];
        return 1;
    }
    CHECK(size == 2);
    ++data_write_count;
    if (fail_data_write != 0 && data_write_count == fail_data_write) {
        errno = EIO;
        fail_data_write = 0;
        return -1;
    }
    registers_by_address[bytes[0]] = bytes[1];
    return 2;
}

ssize_t fake_read(int fd, void *buffer, size_t size)
{
    CHECK(fd == 42);
    CHECK(size == 1);
    *static_cast<uint8_t *>(buffer) = registers_by_address[selected_register];
    return 1;
}

void reset_fake_device(uint8_t output, uint8_t polarity, uint8_t config)
{
    std::memset(registers_by_address, 0, sizeof(registers_by_address));
    registers_by_address[0x01] = output;
    registers_by_address[0x02] = polarity;
    registers_by_address[0x03] = config;
    selected_register = 0;
    data_write_count = 0;
    fail_data_write = 0;
}

} // namespace

#define open fake_open
#define close fake_close
#define ioctl fake_ioctl
#define flock fake_flock
#define write fake_write
#define read fake_read
#include "../src/lora/cp0_lora_pi4io_controller.cpp"
#undef read
#undef write
#undef ioctl
#undef flock
#undef close
#undef open

int main()
{
    using namespace cp0_lora_pi4io_controller;

    reset_fake_device(0xAA, 0x55, 0xF3);
    clear_stop();
    CHECK(scan_and_initialize());
    CHECK(registers_by_address[0x01] == 0xAB);
    CHECK(registers_by_address[0x02] == 0x54);
    CHECK(registers_by_address[0x03] == 0xF2);
    // Changes made by another I2C client while this app is running must
    // survive shutdown; this controller owns only P0 in each register.
    registers_by_address[0x01] = 0xCD;
    registers_by_address[0x02] = 0x32;
    registers_by_address[0x03] = 0x86;
    request_stop();
    shutdown();
    CHECK(registers_by_address[0x01] == 0xCC);
    CHECK(registers_by_address[0x02] == 0x33);
    CHECK(registers_by_address[0x03] == 0x87);
    clear_stop();

    reset_fake_device(0x34, 0xA5, 0x7F);
    fail_data_write = 2;
    CHECK(!scan_and_initialize());
    CHECK(registers_by_address[0x01] == 0x34);
    CHECK(registers_by_address[0x02] == 0xA5);
    CHECK(registers_by_address[0x03] == 0x7F);

    const int opens_before_cancel = open_count;
    request_stop();
    CHECK(!scan_and_initialize());
    CHECK(open_count == opens_before_cancel);
    clear_stop();
    CHECK(open_count == close_count);
}
