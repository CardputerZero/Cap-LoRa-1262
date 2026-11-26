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
    CHECK(step_radio_region({}, true).preset()->code == "US");
    CHECK(step_radio_region({}, false).preset()->code == "PH_915");
    CHECK(step_radio_region(radio_settings_for_code("PH_915"), true).preset()->code == "US");
    for (const auto& region : kRadioRegions) {
        CHECK(radio_settings_for_code(region.code).preset() == &region);
        CHECK(region.fits_cap());
        CHECK(region.tx_power_dbm() <= region.table_power_dbm);
        CHECK(region.tx_power_dbm() <= kCapMaxTxDbm);
    }
    CHECK(radio_settings_for_code("EU_868").preset()->frequency_khz() == 869525);
    CHECK(radio_settings_for_code("EU_N_868").preset()->bandwidth_hz == 62500);
    CHECK(radio_settings_for_code("JP").preset()->tx_power_dbm() == 13);
    CHECK(radio_settings_for_code("TH").preset()->duty_percent == 10);

    LoraDutyCycle duty;
    const auto now = LoraDutyCycle::Clock::time_point{100min + 59s};
    CHECK(!duty.reserve(now, 0us, 10));
    CHECK(!duty.reserve(now, 1min, 10));
    for (int i = 0; i < 180; ++i) CHECK(duty.reserve(now, 2s, 10));
    CHECK(!duty.reserve(now, 1us, 10));
    CHECK(!duty.reserve(now + 61min, 1us, 10));
    CHECK(duty.reserve(now + 61min, 1s, 100));
    CHECK(duty.reserve(now + 62min, 1s, 10));
    return 0;
}
