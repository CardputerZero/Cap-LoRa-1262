#pragma once

// Public lifecycle facade for the Cap LoRa-1262 device.
//
// Hardware access is deferred until setup()/initialize() so constructing a
// view or test object cannot toggle the hat power rail.

#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "pw_result/result.h"
#include "pw_status/status.h"

#include "RadioLib.h"
#include "pw_spi_linux/spi.h"
#include "pw_i2c_linux/initiator.h"
#include "pw_digital_io/polarity.h"
#include "pw_digital_io_linux/digital_io.h"
#include "driver/PI4IO/PI4IOE5V6408_Class.hpp"
#if defined(CAP_LORA_USE_RADIOLIB_PIHAL) && CAP_LORA_USE_RADIOLIB_PIHAL && \
    __has_include("hal/RPi/PiHal.h") && __has_include(<lgpio.h>)
#include "hal/RPi/PiHal.h"
#endif

class Module;
class SX1262;
class MyRadioHal;

namespace cp0_lora_backend {
void gpio_release_all();
}
namespace cp0_lora_hat_power_controller {
void shutdown();
}
namespace cp0_lora_pi4io_controller {
void shutdown();
}

namespace cap_lora {
struct LoraInfo {
    int initialized = 0;
    int hw_ready = 0;
    int tx_mode = 0;
    int tx_in_progress = 0;
    int has_sent_message = 0;
    int rx_event = 0;
    int tx_event = 0;
    char spi_device[64] = {};
    char last_rx[128] = {};
    char last_tx[128] = {};
    char diag[256] = {};
    char probe_summary[256] = {};
    char probe_display[128] = {};
    char pi4io_status[160] = {};
    float rssi = 0.0f;
    float snr = 0.0f;
};
constexpr std::size_t MAX_TEXT_PAYLOAD = 127;
}  // namespace cap_lora

namespace cap_lora {
/*
 使用方式
 CapLoRa1262 lora;
 if(!lora.probe())
    return;
    if(!lora.initialize())
    return;
    lora.set_rx_mode();
    while(true)
    {
        if(!tx_list_is_empty())
        {
            auto msg = lora.poll();
            if(msg.ok())
            {
                std::string msg = msg;
                std::cout << "Received message: " << msg << std::endl;
            }
        }
        else
        {
          lora.set_tx_mode();
          auto msg = tx_list_pop();
          if(!lora.send(msg))
            std::cout << "Send message failed" << std::endl;
          if(!tx_list_is_empty())
            lora.set_rx_mode();
        }
    }
*/
class CapLoRa1262 final {
public:
    CapLoRa1262();
    CapLoRa1262(const CapLoRa1262&) = delete;
    CapLoRa1262& operator=(const CapLoRa1262&) = delete;
    ~CapLoRa1262();

    bool InitHard();
    bool InitModule();
    bool initialize();
    bool probe();
    pw::Result<std::string> poll();
    bool send(const std::string& payload);
    bool set_rx_mode();
    bool set_tx_mode();
    pw::Result<std::string> msg_poll();
    pw::Result<bool> msg_send(const std::string& payload);
    void request_stop() noexcept;
    void clear_stop() noexcept;
    void shutdown();
    bool setup();
    bool StartServer();
    void StopServer();
    void get_info(LoraInfo* info, bool drain_events = false) const;
    bool isReady() const;
    bool exec_send(const char* payload);
    void exec_poll();
    void start_receive();
    void set_tx_mode(bool enabled);

private:
    static constexpr std::size_t kMaxTextPayload = 127;
    std::shared_ptr<pw::spi::LinuxInitiator> spi_initiator_;
    std::shared_ptr<pw::digital_io::LinuxDigitalIoChip> gpio_chip_;
    std::shared_ptr<pw::i2c::LinuxInitiator> I2cInitiator_;
    std::shared_ptr<m5::PI4IOE5V6408_Class> pi4ioe5v6408_;
    std::shared_ptr<MyRadioHal> RadioHal_;
    std::shared_ptr<Module> MyModule_;
    std::shared_ptr<SX1262> sx1262_;
    bool initialized_ = false;
    std::atomic<bool> stop_requested_{false};
    bool tx_in_progress_ = false;
    bool tx_mode_ = false;
    bool has_sent_message_ = false;
    mutable bool rx_pending_ = false;
    mutable bool tx_pending_ = false;
    mutable std::string queued_message_;
    std::string last_tx_;
    std::string spi_device_ = "/dev/spidev0.1";
    float last_rssi_ = 0.0f;
    float last_snr_ = 0.0f;
    inline static std::atomic<bool> received_flag_{false};
    inline static std::atomic<bool> transmitted_flag_{false};
    static void on_packet_received() noexcept { received_flag_.store(true, std::memory_order_release); }
    static void on_packet_transmitted() noexcept { transmitted_flag_.store(true, std::memory_order_release); }
    bool service_radio();
};

}  // namespace cap_lora













namespace cp0_lora_gpio_offset_policy {

constexpr int MIN_OFFSET = 0;
constexpr int MAX_OFFSET = 65535;

enum class Source { DEFAULT_VALUE, ENVIRONMENT, INVALID };

struct Resolution {
    Source source = Source::INVALID;
    int offset = 0;
    bool valid() const { return source != Source::INVALID; }
    bool overridden() const { return source == Source::ENVIRONMENT; }
};

inline Resolution resolve(const char* environment_value, int default_offset)
{
    const auto in_range = [](int value) { return value >= MIN_OFFSET && value <= MAX_OFFSET; };
    if (!environment_value) return in_range(default_offset) ? Resolution{Source::DEFAULT_VALUE, default_offset} : Resolution{};
    if (!environment_value[0]) return {};
    const std::string text(environment_value);
    int parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !in_range(parsed)) return {};
    return {Source::ENVIRONMENT, parsed};
}

}  // namespace cp0_lora_gpio_offset_policy

namespace cp0_lora_device_discovery {
size_t collect_spi_candidates(char out[][64], size_t max_count, const char* preferred);
}  // namespace cp0_lora_device_discovery

namespace cp0_lora_backend {
enum class GpioIrqFdType { NONE, CDEV_EVENT, SYSFS_VALUE };
int gpio_init_output(int gpio, int value);
int gpio_get_value(int gpio);
int gpio_init_output_any(const char* chip_env_name, const char* offset_env_name, int gpio, int value,
                         int* line_fd, const char* line_name);
int gpio_init_input_any(const char* chip_env_name, const char* offset_env_name, int gpio, int* line_fd,
                        const char* line_name);
int gpio_init_input_irq_any(const char* chip_env_name, const char* offset_env_name, int gpio, int* line_fd,
                            const char* line_name, GpioIrqFdType* fd_type = nullptr);
int gpio_get_value_any(int gpio, int line_fd);
int gpio_set_value_any(int gpio, int line_fd, int value);
void gpio_release_all();
bool gpio_open_output_line(const char* chip_path, int offset, int value, int* line_fd);
bool gpio_set_output_line_value(int line_fd, int value);
}  // namespace cp0_lora_backend

namespace cp0_lora_hat_power_controller {
bool enable(int fallback_gpio);
void shutdown();
}  // namespace cp0_lora_hat_power_controller

namespace cp0_lora_pi4io_controller {
bool scan_and_initialize();
void request_stop() noexcept;
void clear_stop() noexcept;
void shutdown();
const char* status();
}  // namespace cp0_lora_pi4io_controller
