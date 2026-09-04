#include "lora/cp0_lora_runtime_policy.hpp"
#include "test_support.hpp"

int main()
{
    using namespace cp0_lora_runtime_policy;

    CHECK(tx_timeout_for_airtime_us(0) == TX_TIMEOUT_MS);
    CHECK(tx_timeout_for_airtime_us(1000000) == TX_TIMEOUT_MS);
    CHECK(tx_timeout_for_airtime_us(5000000) == 9000);
    CHECK(tx_timeout_for_airtime_us(5300000) == 9450);

    CHECK(!should_timeout_transmit(false, true, true, 100, 4100));
    CHECK(!should_timeout_transmit(true, false, true, 100, 4100));
    CHECK(!should_timeout_transmit(true, true, false, 100, 4100));
    CHECK(!should_timeout_transmit(true, true, true, 0, 5000));
    CHECK(!should_timeout_transmit(true, true, true, 100, 4099));
    CHECK(should_timeout_transmit(true, true, true, 100, 4100));
    CHECK(!should_timeout_transmit(true, true, true, 100, 8099, 8000));
    CHECK(should_timeout_transmit(true, true, true, 100, 8100, 8000));
    CHECK(should_timeout_transmit(true, true, true, 100, 4100, 0));
    CHECK(!should_timeout_transmit(true, true, true, 4100, 100));

    CHECK(!should_send_demo(false, true, false, 100, 2100));
    CHECK(!should_send_demo(true, false, false, 100, 2100));
    CHECK(!should_send_demo(true, true, true, 100, 2100));
    CHECK(!should_send_demo(true, true, false, 100, 2099));
    CHECK(should_send_demo(true, true, false, 100, 2100));
    CHECK(!should_send_demo(true, true, false, 2100, 100));

    return 0;
}
