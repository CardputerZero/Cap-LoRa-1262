#include "lora/cap_lora_1262.hpp"
#include "test_support.hpp"

#include <cstring>

int main()
{
    char candidates[8][64] = {};
    size_t count = cp0_lora_device_discovery::collect_spi_candidates(candidates, 8, nullptr);
    CHECK(count == 2);
    CHECK(std::strcmp(candidates[0], "/dev/spidev0.1") == 0);
    CHECK(std::strcmp(candidates[1], "/dev/spidev0.0") == 0);

    std::memset(candidates, 0, sizeof(candidates));
    count = cp0_lora_device_discovery::collect_spi_candidates(candidates, 8, "/dev/spidev7.3");
    CHECK(count == 1);
    CHECK(std::strcmp(candidates[0], "/dev/spidev7.3") == 0);

    count = cp0_lora_device_discovery::collect_spi_candidates(candidates, 0, nullptr);
    CHECK(count == 0);
}
