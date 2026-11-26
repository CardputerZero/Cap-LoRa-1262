#include "lora/lora_duty_cycle.hpp"
#include "lora/lora_radio_settings.hpp"
#include "test_support.hpp"

int main()
{
    using namespace cap_lora;
    using namespace std::chrono_literals;

    CHECK(!radio_settings_for_code("UNSET").complete());
    CHECK(!radio_settings_for_code("EU_866").complete());
    CHECK(!radio_settings_for_code("NZ_865").complete());
    CHECK(!radio_settings_for_code("LORA_24").complete());
    CHECK(step_radio_region({}, true).entry()->code == "US");
    CHECK(step_radio_region({}, false).entry()->code == "NP_865");
    CHECK(step_radio_region(radio_settings_for_code("NP_865"), true).entry()->code == "US");
    CHECK(step_radio_region(radio_settings_for_code("EU_433"), true).entry()->code == "EU_868");
    CHECK(kRadioRegions.size() == 26);
    std::size_t supported = 0;
    for (const auto& region : kRadioRegions) {
        const auto settings = radio_settings_for_code(region.code);
        CHECK(settings.entry() == &region);
        CHECK(settings.complete() == region.fits_cap());
        if (settings.complete()) ++supported;
        CHECK(region.tx_power_dbm() <= region.table_power_dbm);
        CHECK(region.tx_power_dbm() <= kCapMaxTxDbm);
    }
    CHECK(supported == 14);
    CHECK(radio_settings_for_code("EU_866").entry()->duty_permille == 25);
    CHECK(radio_settings_for_code("EU_866").entry()->router_duty_permille == 100);
    CHECK(radio_settings_for_code("NP_865").entry()->upper_khz == 868000);
    CHECK(!radio_settings_for_code("NP_865").preset());
    CHECK(!radio_settings_for_code("CN").preset());
    CHECK(radio_settings_for_code("EU_868").preset()->frequency_khz() == 869525);
    CHECK(radio_settings_for_code("EU_N_868").preset()->bandwidth_hz == 62500);
    CHECK(radio_settings_for_code("JP").preset()->tx_power_dbm() == 13);
    CHECK(radio_settings_for_code("TH").preset()->duty_permille == 100);

    LoraDutyCycle duty;
    const auto now = LoraDutyCycle::Clock::time_point{100min + 59s};
    CHECK(!duty.reserve(now, 0us, 100));
    CHECK(!duty.reserve(now, 1min, 100));
    for (int i = 0; i < 180; ++i) CHECK(duty.reserve(now, 2s, 100));
    CHECK(!duty.reserve(now, 1us, 100));
    CHECK(!duty.reserve(now + 61min, 1us, 100));
    CHECK(duty.reserve(now + 61min, 1s, 1000));
    CHECK(duty.reserve(now + 62min, 1s, 100));
    return 0;
}
