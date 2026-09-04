#include "cp0_lora_gpio.hpp"
#include "cp0_lora_gpio_offset_policy.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <unistd.h>

#if __has_include(<linux/gpio.h>)
#include <linux/gpio.h>
#include <sys/ioctl.h>
#define CP0_LORA_GPIO_HAS_CDEV 1
#else
#define CP0_LORA_GPIO_HAS_CDEV 0
#endif

#ifndef SLOGI
#define SLOGI(...) do { std::printf("[cap_lora] "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#endif

namespace cp0_lora_backend {
namespace {

struct SysfsSnapshot {
    bool exported_by_us = false;
};

std::mutex sysfs_mutex;
std::map<int, SysfsSnapshot> sysfs_snapshots;

int write_text_file(const char *path, const char *value)
{
    if (path == nullptr || value == nullptr) return -1;
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    const size_t length = strlen(value);
    ssize_t result;
    do {
        result = write(fd, value, length);
    } while (result < 0 && errno == EINTR);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return result == static_cast<ssize_t>(length) ? 0 : -1;
}

void gpio_path(char *path, size_t capacity, int gpio, const char *attribute)
{
    snprintf(path, capacity, "/sys/class/gpio/gpio%d/%s", gpio, attribute);
}

bool wait_for_gpio_path(int gpio)
{
    char path[64];
    gpio_path(path, sizeof(path), gpio, "value");
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (access(path, F_OK) == 0) return true;
        usleep(5000);
    }
    return access(path, F_OK) == 0;
}

void restore_snapshot_locked(int gpio, const SysfsSnapshot &snapshot)
{
    char gpio_text[16];
    snprintf(gpio_text, sizeof(gpio_text), "%d", gpio);
    if (snapshot.exported_by_us) (void)write_text_file("/sys/class/gpio/unexport", gpio_text);
}

int acquire_sysfs_locked(int gpio)
{
    if (sysfs_snapshots.find(gpio) != sysfs_snapshots.end()) return 0;

    char value_path[64];
    gpio_path(value_path, sizeof(value_path), gpio, "value");
    const bool already_exported = access(value_path, F_OK) == 0;
    if (already_exported) {
        errno = EBUSY;
        return -1;
    }

    SysfsSnapshot snapshot;
    snapshot.exported_by_us = true;
    char gpio_text[16];
    snprintf(gpio_text, sizeof(gpio_text), "%d", gpio);
    if (write_text_file("/sys/class/gpio/export", gpio_text) < 0) return -1;
    if (!wait_for_gpio_path(gpio)) {
        (void)write_text_file("/sys/class/gpio/unexport", gpio_text);
        return -1;
    }
    sysfs_snapshots.emplace(gpio, std::move(snapshot));
    return 0;
}

int set_direction(int gpio, const char *direction)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    return write_text_file(path, direction);
}

int init_input(int gpio)
{
    std::lock_guard<std::mutex> lock(sysfs_mutex);
    if (acquire_sysfs_locked(gpio) < 0) return -1;
    if (set_direction(gpio, "in") == 0) return 0;
    const auto snapshot = sysfs_snapshots.at(gpio);
    restore_snapshot_locked(gpio, snapshot);
    sysfs_snapshots.erase(gpio);
    return -1;
}

int set_value(int gpio, int value)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return write_text_file(path, value ? "1" : "0");
}

#if !CP0_LORA_GPIO_HAS_CDEV
int init_input_irq_sysfs(int gpio, int *line_fd)
{
    if (line_fd == nullptr || init_input(gpio) < 0) return -1;
    char edge_path[64];
    snprintf(edge_path, sizeof(edge_path), "/sys/class/gpio/gpio%d/edge", gpio);
    if (write_text_file(edge_path, "rising") < 0) {
        gpio_release_sysfs(gpio);
        return -1;
    }
    char value_path[64];
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(value_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        gpio_release_sysfs(gpio);
        return -1;
    }
    char dummy = 0;
    lseek(fd, 0, SEEK_SET);
    (void)read(fd, &dummy, 1);
    *line_fd = fd;
    return 0;
}
#endif

#if CP0_LORA_GPIO_HAS_CDEV
bool open_input_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpiohandle_request request{};
    request.lines = 1;
    request.lineoffsets[0] = (uint32_t)offset;
    request.flags = GPIOHANDLE_REQUEST_INPUT;
    snprintf(request.consumer_label, sizeof(request.consumer_label), "cap-lora-in");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) == 0;
    close(chip_fd);
    if (ok) *line_fd = request.fd;
    return ok;
}

bool get_input_line_value(int line_fd, int *value)
{
    if (line_fd < 0 || value == nullptr) return false;
    gpiohandle_data data{};
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) return false;
    *value = data.values[0] ? 1 : 0;
    return true;
}

bool open_input_event_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpioevent_request request{};
    request.lineoffset = (uint32_t)offset;
    request.handleflags = GPIOHANDLE_REQUEST_INPUT;
    request.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    snprintf(request.consumer_label, sizeof(request.consumer_label), "cap-lora-irq");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &request) == 0;
    close(chip_fd);
    if (!ok) return false;
    const int flags = fcntl(request.fd, F_GETFL, 0);
    if (flags < 0 || fcntl(request.fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(request.fd);
        return false;
    }
    *line_fd = request.fd;
    return true;
}

#endif

} // namespace

int gpio_init_output(int gpio, int value)
{
    std::lock_guard<std::mutex> lock(sysfs_mutex);
    if (acquire_sysfs_locked(gpio) < 0) return -1;
    if (set_direction(gpio, value ? "high" : "low") == 0) return 0;
    if (set_direction(gpio, "out") == 0 && set_value(gpio, value) == 0) return 0;
    const auto snapshot = sysfs_snapshots.at(gpio);
    restore_snapshot_locked(gpio, snapshot);
    sysfs_snapshots.erase(gpio);
    return -1;
}

int gpio_get_value(int gpio)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char value = '0';
    const ssize_t result = read(fd, &value, 1);
    close(fd);
    return result <= 0 ? -1 : (value == '0' ? 0 : 1);
}

bool gpio_open_output_line(const char *chip_path, int offset, int value, int *line_fd)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpiohandle_request request{};
    request.lines = 1;
    request.lineoffsets[0] = (uint32_t)offset;
    request.flags = GPIOHANDLE_REQUEST_OUTPUT;
    request.default_values[0] = (uint8_t)(value ? 1 : 0);
    snprintf(request.consumer_label, sizeof(request.consumer_label), "cap-lora-5v");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) == 0;
    close(chip_fd);
    if (ok) *line_fd = request.fd;
    return ok;
#else
    (void)chip_path; (void)offset; (void)value; (void)line_fd;
    return false;
#endif
}

bool gpio_set_output_line_value(int line_fd, int value)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (line_fd < 0) return false;
    gpiohandle_data data{};
    data.values[0] = (uint8_t)(value ? 1 : 0);
    return ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0;
#else
    (void)line_fd; (void)value;
    return false;
#endif
}

int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name,
                         int gpio, int value, int *line_fd, const char *line_name)
{
    const char *offset_env = offset_env_name ? getenv(offset_env_name) : nullptr;
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    const bool explicit_cdev = (offset_env && offset_env[0]) || (chip_env && chip_env[0]);
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0)
        return gpio_set_output_line_value(*line_fd, value) ? 0 : -1;
#if CP0_LORA_GPIO_HAS_CDEV
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && gpio_open_output_line(chip_path, offset, value, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]=%d", line_name ? line_name : "out", chip_path, offset, value);
        return 0;
    }
#endif
    if (explicit_cdev) {
        SLOGI("LoRa GPIO %s explicit cdev acquisition failed", line_name ? line_name : "out");
        return -1;
    }
    if (gpio_init_output(gpio, value) == 0) return 0;
    SLOGI("LoRa GPIO %s init failed: gpio=%d errno=%d", line_name ? line_name : "out", gpio, errno);
    return -1;
}

int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name,
                        int gpio, int *line_fd, const char *line_name)
{
    const char *offset_env = offset_env_name ? getenv(offset_env_name) : nullptr;
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    const bool explicit_cdev = (offset_env && offset_env[0]) || (chip_env && chip_env[0]);
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0) return 0;
#if CP0_LORA_GPIO_HAS_CDEV
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && open_input_line(chip_path, offset, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]", line_name ? line_name : "in", chip_path, offset);
        return 0;
    }
#endif
    if (explicit_cdev) {
        SLOGI("LoRa GPIO %s explicit cdev acquisition failed", line_name ? line_name : "in");
        return -1;
    }
    if (init_input(gpio) == 0) return 0;
    SLOGI("LoRa GPIO %s input init failed: gpio=%d errno=%d", line_name ? line_name : "in", gpio, errno);
    return -1;
}

int gpio_init_input_irq_any(const char *chip_env_name, const char *offset_env_name,
                            int gpio, int *line_fd, const char *line_name,
                            GpioIrqFdType *fd_type)
{
    if (fd_type) *fd_type = GpioIrqFdType::NONE;
    const char *offset_env = offset_env_name ? getenv(offset_env_name) : nullptr;
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    const bool explicit_cdev = (offset_env && offset_env[0]) || (chip_env && chip_env[0]);
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0) {
#if CP0_LORA_GPIO_HAS_CDEV
        if (fd_type) *fd_type = GpioIrqFdType::CDEV_EVENT;
#else
        if (fd_type) *fd_type = GpioIrqFdType::SYSFS_VALUE;
#endif
        return 0;
    }
#if CP0_LORA_GPIO_HAS_CDEV
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && open_input_event_line(chip_path, offset, line_fd)) {
        if (fd_type) *fd_type = GpioIrqFdType::CDEV_EVENT;
        SLOGI("LoRa GPIO %s irq-event via cdev: %s[%d]", line_name ? line_name : "irq", chip_path, offset);
        return 0;
    }
#endif
#if !CP0_LORA_GPIO_HAS_CDEV
    if (!explicit_cdev && line_fd && init_input_irq_sysfs(gpio, line_fd) == 0) {
        if (fd_type) *fd_type = GpioIrqFdType::SYSFS_VALUE;
        SLOGI("LoRa GPIO %s irq-event via sysfs: gpio%d rising", line_name ? line_name : "irq", gpio);
        return 0;
    }
#else
    (void)explicit_cdev;
#endif
    return -1;
}

int gpio_get_value_any(int gpio, int line_fd)
{
#if CP0_LORA_GPIO_HAS_CDEV
    int value = 0;
    if (line_fd >= 0) return get_input_line_value(line_fd, &value) ? value : -1;
#endif
    return gpio_get_value(gpio);
}

int gpio_set_value_any(int gpio, int line_fd, int value)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (line_fd >= 0) return gpio_set_output_line_value(line_fd, value) ? 0 : -1;
#endif
    return set_value(gpio, value);
}

void gpio_release_sysfs(int gpio)
{
    std::lock_guard<std::mutex> lock(sysfs_mutex);
    const auto found = sysfs_snapshots.find(gpio);
    if (found == sysfs_snapshots.end()) return;
    restore_snapshot_locked(gpio, found->second);
    sysfs_snapshots.erase(found);
}

void gpio_release_all_sysfs()
{
    std::lock_guard<std::mutex> lock(sysfs_mutex);
    for (auto it = sysfs_snapshots.rbegin(); it != sysfs_snapshots.rend(); ++it)
        restore_snapshot_locked(it->first, it->second);
    sysfs_snapshots.clear();
}

} // namespace cp0_lora_backend
