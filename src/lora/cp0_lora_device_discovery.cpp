#include "cp0_lora_device_discovery.hpp"

#include <cstdio>
#include <cstring>

namespace cp0_lora_device_discovery {

size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred)
{
    if (out == nullptr || max_count == 0) return 0;

    size_t count = 0;
    auto append_candidate = [&](const char *path) {
        if (path == nullptr || path[0] == '\0') return;
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(out[i], path) == 0) return;
        }
        if (count < max_count) {
            snprintf(out[count], 64, "%s", path);
            ++count;
        }
    };

    if (preferred != nullptr && preferred[0] != '\0') {
        append_candidate(preferred);
        return count;
    }

    append_candidate("/dev/spidev0.1");
    append_candidate("/dev/spidev0.0");
    return count;
}

} // namespace cp0_lora_device_discovery
