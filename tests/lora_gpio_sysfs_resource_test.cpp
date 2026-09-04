#include "test_support.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <string>
#include <unistd.h>

namespace {

struct GpioState {
    bool exported = false;
    std::string direction = "in";
    std::string value = "0";
    std::string edge = "none";
};

std::map<int, GpioState> gpio_states;
std::map<int, std::string> open_paths;
int next_fd = 100;

bool parse_gpio_attribute(const std::string &path, int *gpio, std::string *attribute)
{
    int parsed_gpio = -1;
    char parsed_attribute[32] = {};
    if (std::sscanf(path.c_str(), "/sys/class/gpio/gpio%d/%31s", &parsed_gpio, parsed_attribute) != 2)
        return false;
    *gpio = parsed_gpio;
    *attribute = parsed_attribute;
    return true;
}

int fake_access(const char *path, int)
{
    int gpio = -1;
    std::string attribute;
    if (!parse_gpio_attribute(path, &gpio, &attribute) || attribute != "value") {
        errno = ENOENT;
        return -1;
    }
    if (gpio_states[gpio].exported) return 0;
    errno = ENOENT;
    return -1;
}

int fake_open(const char *path, int, ...)
{
    const std::string file(path ? path : "");
    if (file == "/sys/class/gpio/export" || file == "/sys/class/gpio/unexport") {
        const int fd = next_fd++;
        open_paths[fd] = file;
        return fd;
    }
    int gpio = -1;
    std::string attribute;
    if (!parse_gpio_attribute(file, &gpio, &attribute) || !gpio_states[gpio].exported) {
        errno = ENOENT;
        return -1;
    }
    const int fd = next_fd++;
    open_paths[fd] = file;
    return fd;
}

int fake_close(int fd)
{
    CHECK(open_paths.erase(fd) == 1);
    return 0;
}

ssize_t fake_read(int fd, void *buffer, size_t size)
{
    const auto found = open_paths.find(fd);
    CHECK(found != open_paths.end());
    int gpio = -1;
    std::string attribute;
    CHECK(parse_gpio_attribute(found->second, &gpio, &attribute));
    const auto &state = gpio_states[gpio];
    const std::string *value = nullptr;
    if (attribute == "direction") value = &state.direction;
    if (attribute == "value") value = &state.value;
    if (attribute == "edge") value = &state.edge;
    CHECK(value != nullptr);
    const size_t count = std::min(size, value->size());
    std::memcpy(buffer, value->data(), count);
    return static_cast<ssize_t>(count);
}

ssize_t fake_write(int fd, const void *buffer, size_t size)
{
    const auto found = open_paths.find(fd);
    CHECK(found != open_paths.end());
    const std::string value(static_cast<const char *>(buffer), size);
    if (found->second == "/sys/class/gpio/export" || found->second == "/sys/class/gpio/unexport") {
        const int gpio = std::stoi(value);
        gpio_states[gpio].exported = found->second == "/sys/class/gpio/export";
        return static_cast<ssize_t>(size);
    }
    int gpio = -1;
    std::string attribute;
    CHECK(parse_gpio_attribute(found->second, &gpio, &attribute));
    auto &state = gpio_states[gpio];
    if (attribute == "direction") {
        if (value == "high" || value == "low") {
            state.direction = "out";
            state.value = value == "high" ? "1" : "0";
        } else {
            state.direction = value;
        }
    } else if (attribute == "value") {
        state.value = value;
    } else if (attribute == "edge") {
        state.edge = value;
    } else {
        CHECK(false);
    }
    return static_cast<ssize_t>(size);
}

int fake_usleep(useconds_t)
{
    return 0;
}

} // namespace

#define access fake_access
#define open fake_open
#define close fake_close
#define read fake_read
#define write fake_write
#define usleep fake_usleep
#include "../src/lora/cp0_lora_gpio.cpp"
#undef usleep
#undef write
#undef read
#undef close
#undef open
#undef access

int main()
{
    gpio_states[26] = {true, "out", "1", "none"};
    CHECK(cp0_lora_backend::gpio_init_output(26, 0) == -1);
    CHECK(errno == EBUSY);
    CHECK(gpio_states[26].direction == "out");
    CHECK(gpio_states[26].value == "1");
    cp0_lora_backend::gpio_release_all_sysfs();
    CHECK(gpio_states[26].exported);
    CHECK(gpio_states[26].direction == "out");
    CHECK(gpio_states[26].value == "1");

    gpio_states[27] = {};
    CHECK(cp0_lora_backend::gpio_init_output(27, 1) == 0);
    CHECK(gpio_states[27].exported);
    CHECK(gpio_states[27].direction == "out");
    CHECK(gpio_states[27].value == "1");
    cp0_lora_backend::gpio_release_all_sysfs();
    CHECK(!gpio_states[27].exported);
    CHECK(open_paths.empty());
}
