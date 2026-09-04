#include "lora/cp0_lora_gpio_offset_policy.hpp"
#include "test_support.hpp"

int main()
{
    using namespace cp0_lora_gpio_offset_policy;

    auto result = resolve(nullptr, 5);
    CHECK(result.valid() && !result.overridden() && result.offset == 5);
    result = resolve("0", 5);
    CHECK(result.valid() && result.overridden() && result.offset == MIN_OFFSET);
    result = resolve("65535", 5);
    CHECK(result.valid() && result.overridden() && result.offset == MAX_OFFSET);

    for (const char *invalid : {"", "-1", "+1", " 5", "5 ", "5junk",
                                "65536", "2147483648", "999999999999999999999"})
        CHECK(!resolve(invalid, 5).valid());
    CHECK(!resolve(nullptr, -1).valid());
    CHECK(!resolve(nullptr, MAX_OFFSET + 1).valid());
}
