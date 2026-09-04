#pragma once

#include <cstddef>

namespace cp0_lora_backend {

enum class GpioIrqFdType
{
    NONE,
    CDEV_EVENT,
    SYSFS_VALUE,
};

int gpio_init_output(int gpio, int value);
int gpio_get_value(int gpio);
int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name,
                         int gpio, int value, int *line_fd, const char *line_name);
int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name,
                        int gpio, int *line_fd, const char *line_name);
int gpio_init_input_irq_any(const char *chip_env_name, const char *offset_env_name,
                            int gpio, int *line_fd, const char *line_name,
                            GpioIrqFdType *fd_type = nullptr);
int gpio_get_value_any(int gpio, int line_fd);
int gpio_set_value_any(int gpio, int line_fd, int value);
void gpio_release_sysfs(int gpio);
void gpio_release_all_sysfs();
bool gpio_open_output_line(const char *chip_path, int offset, int value, int *line_fd);
bool gpio_set_output_line_value(int line_fd, int value);

} // namespace cp0_lora_backend
