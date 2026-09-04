#include "test_support.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <linux/spi/spidev.h>
#include <sys/file.h>

namespace {

int next_fd = 41;
int open_error = 0;
int flock_error = 0;
int ioctl_error_at = 0;
int ioctl_calls = 0;
int close_calls = 0;
int unlock_calls = 0;

int fake_open(const char *path, int flags, ...)
{
    CHECK(std::strcmp(path, "/dev/spidev0.1") == 0);
    CHECK((flags & O_CLOEXEC) != 0);
    if (open_error != 0) {
        errno = open_error;
        return -1;
    }
    return next_fd;
}

int fake_close(int fd)
{
    CHECK(fd == next_fd);
    ++close_calls;
    return 0;
}

int fake_flock(int fd, int operation)
{
    CHECK(fd == next_fd);
    if (operation == LOCK_UN) {
        ++unlock_calls;
        return 0;
    }
    CHECK(operation == (LOCK_EX | LOCK_NB));
    if (flock_error != 0) {
        errno = flock_error;
        return -1;
    }
    return 0;
}

int fake_ioctl(int fd, unsigned long, ...)
{
    CHECK(fd == next_fd);
    ++ioctl_calls;
    if (ioctl_error_at != 0 && ioctl_calls == ioctl_error_at) {
        errno = EIO;
        return -1;
    }
    return 1;
}

void reset_fakes()
{
    open_error = 0;
    flock_error = 0;
    ioctl_error_at = 0;
    ioctl_calls = 0;
    close_calls = 0;
    unlock_calls = 0;
}

} // namespace

#define CP0_LORA_OPEN fake_open
#define CP0_LORA_CLOSE fake_close
#define CP0_LORA_FLOCK fake_flock
#define CP0_LORA_IOCTL fake_ioctl
#include "../src/lora/cp0_lora_spi_device.cpp"
#undef CP0_LORA_IOCTL
#undef CP0_LORA_FLOCK
#undef CP0_LORA_CLOSE
#undef CP0_LORA_OPEN

int main()
{
    cp0_lora::SpiDevice device("/dev/spidev0.1", 8'000'000);
    uint8_t tx[2] = {1, 2};
    uint8_t rx[2] = {};

    reset_fakes();
    CHECK(device.open());
    CHECK(device.is_open());
    CHECK(ioctl_calls == 3);
    CHECK(device.transfer(tx, rx, sizeof(tx)));
    CHECK(ioctl_calls == 4);
    device.close();
    CHECK(close_calls == 1 && unlock_calls == 1);

    reset_fakes();
    flock_error = EWOULDBLOCK;
    CHECK(!device.open());
    CHECK(device.last_error() == EWOULDBLOCK);
    CHECK(close_calls == 1);

    reset_fakes();
    ioctl_error_at = 2;
    CHECK(!device.open());
    CHECK(device.last_error() == EIO);
    CHECK(close_calls == 1 && unlock_calls == 1);

    reset_fakes();
    CHECK(!device.transfer(tx, rx, sizeof(tx)));
    CHECK(device.last_error() == EBADF);
    CHECK(device.open());
    CHECK(!device.transfer(tx, rx, static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1));
    CHECK(device.last_error() == EOVERFLOW);
    device.close();
}
