#include "../src/lora/cp0_lora_gpio.hpp"
#include "../src/lora/cp0_lora_gpio_offset_policy.hpp"
#include "test_support.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace {

std::string brightness = "0";
bool led_available = true;
int led_open_count = 0;
int cdev_open_count = 0;
int cdev_value = -1;

int fake_open(const char *path, int, ...)
{
    CHECK(std::strcmp(path, "/sys/class/leds/ext_5v_out/brightness") == 0);
    ++led_open_count;
    if (!led_available) {
        errno = ENOENT;
        return -1;
    }
    return 51;
}

int fake_close(int fd)
{
    CHECK(fd == 51 || fd == 77);
    return 0;
}

ssize_t fake_read(int fd, void *buffer, size_t size)
{
    CHECK(fd == 51 && size > brightness.size());
    std::memcpy(buffer, brightness.data(), brightness.size());
    return static_cast<ssize_t>(brightness.size());
}

ssize_t fake_write(int fd, const void *buffer, size_t size)
{
    CHECK(fd == 51);
    brightness.assign(static_cast<const char *>(buffer), size);
    return static_cast<ssize_t>(size);
}

} // namespace

#define open fake_open
#define close fake_close
#define read fake_read
#define write fake_write
#include "../src/lora/cp0_lora_hat_power_controller.cpp"
#undef write
#undef read
#undef close
#undef open

namespace cp0_lora_backend {

bool gpio_open_output_line(const char *, int, int, int *line_fd)
{
    ++cdev_open_count;
    *line_fd = 77;
    return true;
}

bool gpio_set_output_line_value(int line_fd, int value)
{
    CHECK(line_fd == 77);
    cdev_value = value;
    return true;
}

} // namespace cp0_lora_backend

int main()
{
    using namespace cp0_lora_hat_power_controller;

    unsetenv("HAT_5VOUT_CHIP");
    unsetenv("HAT_5VOUT_OFFSET");
    CHECK(enable(5));
    CHECK(brightness == "1");
    shutdown();
    CHECK(brightness == "0");

    led_available = false;
    CHECK(!enable(5));
    CHECK(cdev_open_count == 0);

    const int led_opens_before_override = led_open_count;
    setenv("HAT_5VOUT_CHIP", "/dev/gpiochip9", 1);
    setenv("HAT_5VOUT_OFFSET", "12", 1);
    CHECK(enable(5));
    CHECK(cdev_open_count == 1 && cdev_value == 0);
    CHECK(led_open_count == led_opens_before_override);
    shutdown();
    CHECK(cdev_value == 1);

    unsetenv("HAT_5VOUT_OFFSET");
    CHECK(!enable(5));
    CHECK(cdev_open_count == 1);
    unsetenv("HAT_5VOUT_CHIP");
}
