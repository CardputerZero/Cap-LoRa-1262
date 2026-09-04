#include "cp0_lora_spi_device.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/file.h>
#include <unistd.h>

#if __has_include(<sys/ioctl.h>) && __has_include(<linux/spi/spidev.h>)
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#else
extern "C" int ioctl(int fd, unsigned long request, ...);
struct spi_ioc_transfer {
    unsigned long tx_buf;
    unsigned long rx_buf;
    uint32_t len;
    uint32_t speed_hz;
    uint16_t delay_usecs;
    uint8_t bits_per_word;
    uint8_t cs_change;
    uint32_t pad;
};
#ifndef SPI_MODE_0
#define SPI_MODE_0 0
#endif

#ifndef SPI_IOC_WR_MODE
#define SPI_IOC_WR_MODE 0
#endif
#ifndef SPI_IOC_WR_BITS_PER_WORD
#define SPI_IOC_WR_BITS_PER_WORD 0
#endif
#ifndef SPI_IOC_WR_MAX_SPEED_HZ
#define SPI_IOC_WR_MAX_SPEED_HZ 0
#endif
#ifndef SPI_IOC_MESSAGE
#define SPI_IOC_MESSAGE(N) 0
#endif
#endif

#ifndef CP0_LORA_OPEN
#define CP0_LORA_OPEN ::open
#endif
#ifndef CP0_LORA_CLOSE
#define CP0_LORA_CLOSE ::close
#endif
#ifndef CP0_LORA_FLOCK
#define CP0_LORA_FLOCK ::flock
#endif
#ifndef CP0_LORA_IOCTL
#define CP0_LORA_IOCTL ::ioctl
#endif

namespace cp0_lora {

SpiDevice::SpiDevice(const char *path, uint32_t speed_hz) : speed_hz_(speed_hz)
{
    set_path(path);
}

SpiDevice::~SpiDevice()
{
    close();
}

const char *SpiDevice::path() const
{
    return path_;
}

void SpiDevice::set_path(const char *path)
{
    if (is_open())
        close();
    std::snprintf(path_, sizeof(path_), "%s", path ? path : "");
}

bool SpiDevice::is_open() const
{
    return fd_ >= 0;
}

int SpiDevice::last_error() const
{
    return last_error_;
}

bool SpiDevice::open()
{
    if (is_open())
        return true;
    last_error_ = 0;
    fd_ = CP0_LORA_OPEN(path_, O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        last_error_ = errno;
        return false;
    }

    if (CP0_LORA_FLOCK(fd_, LOCK_EX | LOCK_NB) < 0) {
        last_error_ = errno;
        close();
        return false;
    }

    uint8_t mode = static_cast<uint8_t>(SPI_MODE_0);
    uint8_t bits = 8;
    if (CP0_LORA_IOCTL(fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
        CP0_LORA_IOCTL(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        CP0_LORA_IOCTL(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
        last_error_ = errno;
        close();
        return false;
    }
    return true;
}

void SpiDevice::close()
{
    if (fd_ >= 0) {
        (void)CP0_LORA_FLOCK(fd_, LOCK_UN);
        CP0_LORA_CLOSE(fd_);
        fd_ = -1;
    }
}

bool SpiDevice::transfer(const uint8_t *tx, uint8_t *rx, size_t length)
{
    if (!is_open()) {
        last_error_ = EBADF;
        return false;
    }
    if (length > std::numeric_limits<uint32_t>::max()) {
        last_error_ = EOVERFLOW;
        return false;
    }
    if (length == 0) {
        last_error_ = 0;
        return true;
    }
    struct spi_ioc_transfer transfer;
    std::memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = reinterpret_cast<unsigned long>(tx);
    transfer.rx_buf = reinterpret_cast<unsigned long>(rx);
    transfer.len = static_cast<uint32_t>(length);
    transfer.speed_hz = speed_hz_;
    transfer.bits_per_word = 8;
    if (CP0_LORA_IOCTL(fd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        last_error_ = errno;
        return false;
    }
    last_error_ = 0;
    return true;
}

} // namespace cp0_lora
