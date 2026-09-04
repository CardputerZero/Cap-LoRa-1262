#include "cp0_lora_pi4io_controller.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
#include <sys/file.h>
#include <unistd.h>

#if __has_include(<linux/i2c-dev.h>)
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#define CP0_PI4IO_HAS_LINUX_I2CDEV 1
#else
#define CP0_PI4IO_HAS_LINUX_I2CDEV 0
#endif

namespace cp0_lora_pi4io_controller {
namespace {

constexpr int I2C_BUS = 1;
constexpr int SDA_GPIO = 2;
constexpr int SCL_GPIO = 3;
constexpr uint8_t I2C_ADDRESS = 0x43;
constexpr int I2C_TIMEOUT_10MS_UNITS = 10;
constexpr auto OPERATION_BUDGET = std::chrono::milliseconds(1200);

char status_text[160] = "I2C 0x43 not checked";
uint8_t output_cache = 0x00;
uint8_t config_cache = 0xFF;
uint8_t polarity_cache = 0x00;
std::mutex controller_mutex;
std::atomic<bool> stop_requested{false};

struct RegisterSnapshot {
    bool valid = false;
    uint8_t output = 0;
    uint8_t config = 0;
    uint8_t polarity = 0;
};

RegisterSnapshot saved_registers;

using Deadline = std::chrono::steady_clock::time_point;

bool should_stop(const Deadline &deadline)
{
    return stop_requested.load(std::memory_order_acquire) ||
           std::chrono::steady_clock::now() >= deadline;
}

bool open_bus(int *fd)
{
#if !CP0_PI4IO_HAS_LINUX_I2CDEV
    if (fd) *fd = -1;
    snprintf(status_text, sizeof(status_text),
             "I2C dev header missing, cannot access 0x%02X", I2C_ADDRESS);
    return false;
#else
    if (fd == nullptr) {
        snprintf(status_text, sizeof(status_text), "I2C fd pointer invalid");
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "/dev/i2c-%d", I2C_BUS);
    *fd = open(path, O_RDWR | O_CLOEXEC);
    if (*fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "open %s failed, SDA:%d SCL:%d errno=%d",
                 path, SDA_GPIO, SCL_GPIO, errno);
        return false;
    }
    if (flock(*fd, LOCK_EX | LOCK_NB) < 0) {
        const int saved_errno = errno;
        snprintf(status_text, sizeof(status_text),
                 "lock %s failed errno=%d", path, saved_errno);
        close(*fd);
        *fd = -1;
        errno = saved_errno;
        return false;
    }
    if (ioctl(*fd, I2C_TIMEOUT, I2C_TIMEOUT_10MS_UNITS) < 0 ||
        ioctl(*fd, I2C_RETRIES, 0) < 0) {
        const int saved_errno = errno;
        snprintf(status_text, sizeof(status_text),
                 "configure %s timeout failed errno=%d", path, saved_errno);
        close(*fd);
        *fd = -1;
        errno = saved_errno;
        return false;
    }
    return true;
#endif
}

bool select_device(int fd)
{
    if (fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "I2C fd invalid for 0x%02X", I2C_ADDRESS);
        return false;
    }
#if CP0_PI4IO_HAS_LINUX_I2CDEV
    if (ioctl(fd, I2C_SLAVE, I2C_ADDRESS) < 0) {
        snprintf(status_text, sizeof(status_text),
                 "select 0x%02X failed on /dev/i2c-%d errno=%d",
                 I2C_ADDRESS, I2C_BUS, errno);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool write_register(int fd, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return write(fd, data, sizeof(data)) == (ssize_t)sizeof(data);
}

bool read_register(int fd, uint8_t reg, uint8_t *value)
{
    if (value == nullptr || write(fd, &reg, 1) != 1) return false;
    return read(fd, value, 1) == 1;
}

bool probe(int fd)
{
    uint8_t reg = 0x00;
    if (write(fd, &reg, 1) != 1) {
        snprintf(status_text, sizeof(status_text),
                 "I2C 0x%02X not found on /dev/i2c-%d (SDA:%d SCL:%d)",
                 I2C_ADDRESS, I2C_BUS, SDA_GPIO, SCL_GPIO);
        return false;
    }
    snprintf(status_text, sizeof(status_text),
             "I2C 0x%02X found on /dev/i2c-%d (SDA:%d SCL:%d)",
             I2C_ADDRESS, I2C_BUS, SDA_GPIO, SCL_GPIO);
    return true;
}

bool restore_registers(int fd, const RegisterSnapshot &snapshot)
{
    if (!snapshot.valid) return true;
    bool ok = true;
    const struct {
        uint8_t reg;
        uint8_t saved;
    } registers[] = {
        {0x01, snapshot.output},
        {0x02, snapshot.polarity},
        {0x03, snapshot.config},
    };
    for (const auto &item : registers) {
        uint8_t current = 0;
        if (!read_register(fd, item.reg, &current)) {
            ok = false;
            continue;
        }
        const uint8_t restored = static_cast<uint8_t>((current & ~uint8_t{0x01}) |
                                                       (item.saved & uint8_t{0x01}));
        if (!write_register(fd, item.reg, restored)) ok = false;
    }
    return ok;
}

bool initialize(int fd, const Deadline &deadline)
{
    if (fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "I2C IO init invalid fd for 0x%02X", I2C_ADDRESS);
        return false;
    }

    RegisterSnapshot original;
    original.valid = true;
    if (should_stop(deadline) || !read_register(fd, 0x01, &original.output) ||
        should_stop(deadline) || !read_register(fd, 0x02, &original.polarity) ||
        should_stop(deadline) || !read_register(fd, 0x03, &original.config)) {
        snprintf(status_text, sizeof(status_text),
                 "I2C IO snapshot failed at 0x%02X errno=%d", I2C_ADDRESS, errno);
        return false;
    }

    polarity_cache = static_cast<uint8_t>(original.polarity & ~uint8_t{0x01});
    output_cache = static_cast<uint8_t>(original.output | uint8_t{0x01});
    config_cache = static_cast<uint8_t>(original.config & ~uint8_t{0x01});
    struct RegisterWrite {
        uint8_t reg;
        uint8_t value;
        const char *name;
    };
    const RegisterWrite writes[] = {
        {0x02, polarity_cache, "POL"},
        {0x01, output_cache, "OUT"},
        {0x03, config_cache, "CFG"},
    };
    for (const auto &item : writes) {
        if (should_stop(deadline)) {
            snprintf(status_text, sizeof(status_text), "I2C IO init cancelled");
            (void)restore_registers(fd, original);
            return false;
        }
        errno = 0;
        if (!write_register(fd, item.reg, item.value)) {
            snprintf(status_text, sizeof(status_text),
                     "I2C IO write %s failed at 0x%02X errno=%d",
                     item.name, I2C_ADDRESS, errno);
            (void)restore_registers(fd, original);
            return false;
        }
    }

    saved_registers = original;

    snprintf(status_text, sizeof(status_text),
             "I2C IO init ok OUT=0x%02X POL=0x%02X CFG=0x%02X P0=HIGH",
             output_cache, polarity_cache, config_cache);
    return true;
}

} // namespace

bool scan_and_initialize()
{
    std::lock_guard<std::mutex> lock(controller_mutex);
    if (saved_registers.valid) return true;
    const Deadline deadline = std::chrono::steady_clock::now() + OPERATION_BUDGET;
    if (should_stop(deadline)) {
        snprintf(status_text, sizeof(status_text), "I2C IO init cancelled");
        return false;
    }
    int fd = -1;
    if (!open_bus(&fd)) return false;
    const bool ok = !should_stop(deadline) && select_device(fd) &&
                    !should_stop(deadline) && probe(fd) &&
                    !should_stop(deadline) && initialize(fd, deadline);
    close(fd);
    return ok;
}

void request_stop() noexcept
{
    stop_requested.store(true, std::memory_order_release);
}

void clear_stop() noexcept
{
    stop_requested.store(false, std::memory_order_release);
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(controller_mutex);
    if (!saved_registers.valid) return;
    int fd = -1;
    if (!open_bus(&fd)) return;
    const bool ok = select_device(fd) && restore_registers(fd, saved_registers);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    if (ok) {
        saved_registers = {};
        snprintf(status_text, sizeof(status_text), "I2C IO state restored");
    } else {
        snprintf(status_text, sizeof(status_text),
                 "I2C IO restore failed at 0x%02X errno=%d", I2C_ADDRESS, errno);
    }
}

const char *status()
{
    return status_text;
}

} // namespace cp0_lora_pi4io_controller
