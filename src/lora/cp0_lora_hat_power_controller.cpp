#include "cp0_lora_hat_power_controller.hpp"

#include "cp0_lora_gpio.hpp"
#include "cp0_lora_gpio_offset_policy.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

#ifndef SLOGI
#define SLOGI(...) do { std::printf("[cap_lora] "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#endif

#if __has_include(<linux/gpio.h>)
#define CP0_HAT_POWER_HAS_GPIO_CDEV 1
#else
#define CP0_HAT_POWER_HAS_GPIO_CDEV 0
#endif

namespace cp0_lora_hat_power_controller {
namespace {

constexpr const char *LED_BRIGHTNESS_PATH = "/sys/class/leds/ext_5v_out/brightness";

enum class Backend
{
    NONE,
    LED,
    CDEV,
};

std::mutex power_mutex;
Backend backend = Backend::NONE;
int line_fd = -1;
int line_offset = 5;
char chip_path[64] = "";
std::string saved_brightness;

bool read_text_file(const char *path, std::string *value)
{
    if (path == nullptr || value == nullptr) return false;
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buffer[32] = {};
    ssize_t count;
    do {
        count = read(fd, buffer, sizeof(buffer) - 1);
    } while (count < 0 && errno == EINTR);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    if (count <= 0) return false;
    while (count > 0 && (buffer[count - 1] == '\n' || buffer[count - 1] == '\r')) --count;
    value->assign(buffer, static_cast<size_t>(count));
    return !value->empty();
}

bool write_text_file(const char *path, const char *value)
{
    if (path == nullptr || value == nullptr) return false;
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const size_t length = strlen(value);
    ssize_t count;
    do {
        count = write(fd, value, length);
    } while (count < 0 && errno == EINTR);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return count == static_cast<ssize_t>(length);
}

void log_result(const char *stage, bool cdev_ok)
{
    const char *chip = chip_path[0] ? chip_path : "sysfs";
    SLOGI("5VDBG %s cdev=%s chip=%s[%d]", stage ? stage : "?",
          cdev_ok ? "ok" : "fail", chip, line_offset);
}

enum class PrepareResult
{
    READY,
    UNAVAILABLE,
    INVALID_OFFSET,
};

PrepareResult prepare_line(bool explicit_override)
{
#if CP0_HAT_POWER_HAS_GPIO_CDEV
    const char *chip_env = getenv("HAT_5VOUT_CHIP");
    const char *offset_env = getenv("HAT_5VOUT_OFFSET");
    if (!explicit_override || !chip_env || !chip_env[0] || !offset_env || !offset_env[0] ||
        strlen(chip_env) >= sizeof(chip_path))
        return PrepareResult::INVALID_OFFSET;
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, 5);
    if (!offset_resolution.valid()) return PrepareResult::INVALID_OFFSET;
    snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    line_offset = offset_resolution.offset;
    if (line_fd >= 0) return PrepareResult::READY;
    return cp0_lora_backend::gpio_open_output_line(
        chip_path, line_offset, 1, &line_fd) ? PrepareResult::READY
                                             : PrepareResult::UNAVAILABLE;
#else
    (void)explicit_override;
    return PrepareResult::UNAVAILABLE;
#endif
}

} // namespace

bool enable(int fallback_gpio)
{
    std::lock_guard<std::mutex> lock(power_mutex);
    (void)fallback_gpio;
    if (backend != Backend::NONE) return true;

    const bool explicit_override = getenv("HAT_5VOUT_CHIP") != nullptr ||
                                   getenv("HAT_5VOUT_OFFSET") != nullptr;
    if (!explicit_override && read_text_file(LED_BRIGHTNESS_PATH, &saved_brightness)) {
        if (write_text_file(LED_BRIGHTNESS_PATH, "1")) {
            backend = Backend::LED;
            SLOGI("5VDBG enabled via %s (saved=%s)", LED_BRIGHTNESS_PATH,
                  saved_brightness.c_str());
            usleep(50000);
            return true;
        }
        saved_brightness.clear();
        SLOGI("5VDBG write %s failed errno=%d", LED_BRIGHTNESS_PATH, errno);
        return false;
    }
    if (!explicit_override) {
        SLOGI("5VDBG %s unavailable; refusing implicit GPIO fallback", LED_BRIGHTNESS_PATH);
        return false;
    }

#if CP0_HAT_POWER_HAS_GPIO_CDEV
    const PrepareResult prepared = prepare_line(explicit_override);
    if (prepared == PrepareResult::INVALID_OFFSET) {
        SLOGI("5VDBG invalid/incomplete HAT_5VOUT_CHIP+HAT_5VOUT_OFFSET override");
        return false;
    }
    if (prepared == PrepareResult::READY &&
        cp0_lora_backend::gpio_set_output_line_value(line_fd, 0)) {
        backend = Backend::CDEV;
        log_result("cdev_set", true);
        usleep(50000);
        return true;
    }
#else
    (void)explicit_override;
#endif
    if (line_fd >= 0) {
        close(line_fd);
        line_fd = -1;
    }
    SLOGI("5VDBG no safe HAT power control path; refusing GPIO5 fallback");
    return false;
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(power_mutex);
    if (backend == Backend::LED) {
        if (!saved_brightness.empty() &&
            !write_text_file(LED_BRIGHTNESS_PATH, saved_brightness.c_str()))
            SLOGI("5VDBG restore %s failed errno=%d", LED_BRIGHTNESS_PATH, errno);
    }
    if (line_fd >= 0) {
        if (!cp0_lora_backend::gpio_set_output_line_value(line_fd, 1))
            SLOGI("5VDBG disable cdev line failed errno=%d", errno);
        close(line_fd);
        line_fd = -1;
    }
    backend = Backend::NONE;
    saved_brightness.clear();
    chip_path[0] = '\0';
    line_offset = 5;
}

} // namespace cp0_lora_hat_power_controller
