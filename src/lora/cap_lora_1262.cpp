// Single translation unit for the Cap LoRa-1262 device and its platform helpers.

#include "cap_lora_1262.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include "pw_digital_io/polarity.h"
#include "pw_digital_io_linux/digital_io.h"

class MyRadioHal final : public RadioLibHal {
    public:
    enum class PinNum : int {
    //   \param hal A Hardware abstraction layer instance. An ArduinoHal instance for example.
    //   \param cs Pin to be used as chip select.
    //   \param irq Pin to be used as interrupt/GPIO.
    //   \param rst Pin to be used as hardware reset for the module.
    //   \param gpio Pin to be used as additional interrupt/GPIO.
        CS = 0,
        IRQ = 23,
        RST = 26,
        GPIO = 22,
    };
private:
    std::shared_ptr<pw::spi::LinuxInitiator> spi_initiator_;
    std::shared_ptr<pw::digital_io::LinuxDigitalIoChip> gpio_chip_;
    int reset_fd_ = -1;
    int busy_fd_ = -1;
    int irq_fd_ = -1;

public:
    MyRadioHal(const std::shared_ptr<pw::spi::LinuxInitiator>& spi_initiator,
               const std::shared_ptr<pw::digital_io::LinuxDigitalIoChip>& gpio_chip)
        : RadioLibHal(
              /* input */ 0,
              /* output */ 1,
              /* low */ 0,
              /* high */ 1,
              /* rising */ 1,
              /* falling */ 2),
          spi_initiator_(spi_initiator),
          gpio_chip_(gpio_chip)
    {
    }
    bool probe()
    {
        if (!spi_initiator_ || !gpio_chip_) {
            return false;
        }
        return true;
    }
    void pinMode(uint32_t pin, uint32_t mode) override
    {
        if (pin == RADIOLIB_NC) return;
        if (mode == GpioModeOutput) {
            (void)cp0_lora_backend::gpio_init_output_any(nullptr, nullptr, static_cast<int>(pin), 1,
                                                         pin == static_cast<uint32_t>(PinNum::RST) ? &reset_fd_ : nullptr,
                                                         "RadioLib output");
            return;
        }
        int* line_fd = pin == static_cast<uint32_t>(PinNum::GPIO) ? &busy_fd_
                       : pin == static_cast<uint32_t>(PinNum::IRQ) ? &irq_fd_
                                                                  : nullptr;
        (void)cp0_lora_backend::gpio_init_input_any(nullptr, nullptr, static_cast<int>(pin), line_fd,
                                                    "RadioLib input");
    }
    void digitalWrite(uint32_t pin, uint32_t value) override
    {
        if (pin == RADIOLIB_NC) return;
        const int line_fd = pin == static_cast<uint32_t>(PinNum::RST) ? reset_fd_ : -1;
        (void)cp0_lora_backend::gpio_set_value_any(static_cast<int>(pin), line_fd, value ? 1 : 0);
    }
    uint32_t digitalRead(uint32_t pin) override
    {
        if (pin == RADIOLIB_NC) return 0;
        const int line_fd = pin == static_cast<uint32_t>(PinNum::GPIO) ? busy_fd_
                            : pin == static_cast<uint32_t>(PinNum::IRQ) ? irq_fd_
                                                                       : -1;
        return cp0_lora_backend::gpio_get_value_any(static_cast<int>(pin), line_fd) > 0 ? 1U : 0U;
    }

    void attachInterrupt(uint32_t, void (*)(void), uint32_t) override
    {
    }
    void detachInterrupt(uint32_t) override
    {
    }

    void delay(RadioLibTime_t milliseconds) override
    {
        ::usleep(static_cast<useconds_t>(milliseconds) * 1000U);
    }
    void delayMicroseconds(RadioLibTime_t microseconds) override
    {
        ::usleep(static_cast<useconds_t>(microseconds));
    }
    RadioLibTime_t millis() override
    {
        struct timespec time;
        ::clock_gettime(CLOCK_MONOTONIC, &time);
        return static_cast<RadioLibTime_t>(time.tv_sec * 1000ULL + time.tv_nsec / 1000000ULL);
    }
    RadioLibTime_t micros() override
    {
        struct timespec time;
        ::clock_gettime(CLOCK_MONOTONIC, &time);
        return static_cast<RadioLibTime_t>(time.tv_sec * 1000000ULL + time.tv_nsec / 1000ULL);
    }
    long pulseIn(uint32_t, uint32_t, RadioLibTime_t) override
    {
        return 0;
    }

    void spiBegin() override
    {
    }
    void spiBeginTransaction() override
    {
    }
    void spiTransfer(uint8_t* tx, size_t len, uint8_t* rx) override
    {
        if (!spi_initiator_) {
            if (rx) std::memset(rx, 0, len);
            return;
        }
        uint8_t zeros[512] = {};
        if (len > sizeof(zeros)) {
            if (rx) std::memset(rx, 0, len);
            return;
        }
        const uint8_t* output = tx ? tx : zeros;
        uint8_t* input        = rx ? rx : zeros;
        const pw::ConstByteSpan output_bytes(reinterpret_cast<const std::byte*>(output), len);
        const pw::ByteSpan input_bytes(reinterpret_cast<std::byte*>(input), len);
        if (!spi_initiator_->WriteRead(output_bytes, input_bytes).ok() && rx) std::memset(rx, 0, len);
    }
    void spiEndTransaction() override
    {
    }
    void spiEnd() override
    {
    }
};

bool cap_lora::CapLoRa1262::InitHard()
{
    if (!cp0_lora_hat_power_controller::enable(5)) return false;
    // Let the controller snapshot and restore the expander registers. Direct
    // writes here would leave P0 configured after the application exits.
    if (!cp0_lora_pi4io_controller::scan_and_initialize()) {
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }
    auto i2c = pw::i2c::LinuxInitiator::OpenI2cBus("/dev/i2c-1");
    if (!i2c.ok()) {
        cp0_lora_pi4io_controller::shutdown();
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }
    I2cInitiator_ = std::make_shared<pw::i2c::LinuxInitiator>(std::move(*i2c));

    pi4ioe5v6408_ = std::make_shared<m5::PI4IOE5V6408_Class>(I2cInitiator_);
    if (!pi4ioe5v6408_) {
        I2cInitiator_.reset();
        cp0_lora_pi4io_controller::shutdown();
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }
    char candidates[8][64] = {};
    const size_t candidate_count = cp0_lora_device_discovery::collect_spi_candidates(
        candidates, 8, std::getenv("LORA_SPI_DEV"));
    int spi_fd = -1;
    for (size_t i = 0; i < candidate_count; ++i) {
        spi_fd = ::open(candidates[i], O_RDWR | O_CLOEXEC);
        if (spi_fd >= 0) {
            spi_device_ = candidates[i];
            break;
        }
    }
    if (spi_fd < 0) {
        pi4ioe5v6408_.reset();
        I2cInitiator_.reset();
        cp0_lora_pi4io_controller::shutdown();
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }
    spi_initiator_ = std::make_shared<pw::spi::LinuxInitiator>(spi_fd, 1000000);
    const pw::spi::Config config{pw::spi::ClockPolarity::kActiveHigh, pw::spi::ClockPhase::kRisingEdge,
                                 pw::spi::BitsPerWord(8), pw::spi::BitOrder::kMsbFirst};
    if (!spi_initiator_->Configure(config).ok()) {
        spi_initiator_.reset();
        I2cInitiator_.reset();
        pi4ioe5v6408_.reset();
        cp0_lora_pi4io_controller::shutdown();
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }

    auto gpio = pw::digital_io::LinuxDigitalIoChip::Open("/dev/gpiochip0");
    if (!gpio.ok()) {
        spi_initiator_.reset();
        I2cInitiator_.reset();
        pi4ioe5v6408_.reset();
        cp0_lora_pi4io_controller::shutdown();
        cp0_lora_hat_power_controller::shutdown();
        return false;
    }
    gpio_chip_ = std::make_shared<pw::digital_io::LinuxDigitalIoChip>(std::move(*gpio));

    return true;
}
bool cap_lora::CapLoRa1262::InitModule()
{
    RadioHal_ = std::make_shared<MyRadioHal>(spi_initiator_, gpio_chip_);
    MyModule_ = std::make_shared<Module>(RadioHal_.get(), RADIOLIB_NC, 23, 26, 22);
    MyModule_->spiConfig.timeout = 100;
    sx1262_   = std::make_shared<SX1262>(MyModule_.get());
    return true;
}

bool cap_lora::CapLoRa1262::probe()
{
    if (!I2cInitiator_ && !InitHard()) return false;
    if (!MyModule_ && !InitModule()) return false;
    return pi4ioe5v6408_ && RadioHal_->probe();
}
cap_lora::CapLoRa1262::CapLoRa1262() = default;
cap_lora::CapLoRa1262::~CapLoRa1262() { shutdown(); }

bool cap_lora::CapLoRa1262::service_radio()
{
    if (!sx1262_) return false;
    const uint32_t irq_flags = sx1262_->getIrqFlags();
    const bool tx_done = transmitted_flag_.exchange(false, std::memory_order_acq_rel) ||
                         ((irq_flags & RADIOLIB_SX126X_IRQ_TX_DONE) != 0);
    if (tx_in_progress_ && tx_done) {
        const int16_t result = sx1262_->finishTransmit();
        tx_in_progress_ = false;
        tx_mode_ = false;
        tx_pending_ = result == RADIOLIB_ERR_NONE;
        if (result != RADIOLIB_ERR_NONE) return false;
        return sx1262_->startReceive() == RADIOLIB_ERR_NONE;
    }
    const bool rx_done = received_flag_.exchange(false, std::memory_order_acq_rel) ||
                         ((irq_flags & RADIOLIB_SX126X_IRQ_RX_DONE) != 0);
    if (!rx_done || tx_in_progress_) return tx_pending_;
    uint8_t buffer[128] = {};
    const size_t length = sx1262_->getPacketLength();
    const size_t read_length = length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;
    if (sx1262_->readData(buffer, read_length) != RADIOLIB_ERR_NONE) return false;
    queued_message_.assign(reinterpret_cast<const char*>(buffer), read_length);
    last_rssi_ = sx1262_->getRSSI();
    last_snr_ = sx1262_->getSNR();
    rx_pending_ = !queued_message_.empty();
    (void)sx1262_->startReceive();
    return true;
}

bool cap_lora::CapLoRa1262::initialize()
{
    if (stop_requested_.load(std::memory_order_acquire)) return false;
    if (!sx1262_ && !probe()) return false;
    if (sx1262_->begin(868.0f, 125.0f, 12, 5, 0x34, 22, 20, 3.0f, false) != RADIOLIB_ERR_NONE) return false;
    if (sx1262_->setCurrentLimit(140) != RADIOLIB_ERR_NONE || sx1262_->setDio2AsRfSwitch(true) != RADIOLIB_ERR_NONE)
        return false;
    sx1262_->setPacketReceivedAction(&CapLoRa1262::on_packet_received);
    sx1262_->setPacketSentAction(&CapLoRa1262::on_packet_transmitted);
    initialized_ = true;
    tx_in_progress_ = false;
    tx_mode_ = false;
    has_sent_message_ = false;
    rx_pending_ = false;
    tx_pending_ = false;
    queued_message_.clear();
    last_tx_.clear();
    received_flag_.store(false, std::memory_order_release);
    transmitted_flag_.store(false, std::memory_order_release);
    return set_rx_mode();
}

bool cap_lora::CapLoRa1262::set_rx_mode()
{
    if (!initialized_ || !sx1262_ || tx_in_progress_) return false;
    if (sx1262_->startReceive() != RADIOLIB_ERR_NONE) return false;
    tx_mode_ = false;
    return true;
}

bool cap_lora::CapLoRa1262::set_tx_mode()
{
    if (!initialized_ || !sx1262_ || tx_in_progress_) return false;
    if (sx1262_->standby() != RADIOLIB_ERR_NONE) return false;
    tx_mode_ = true;
    return true;
}

pw::Result<std::string> cap_lora::CapLoRa1262::msg_poll()
{
    if (!initialized_) return pw::Status::Unavailable();
    (void)service_radio();
    if (!rx_pending_ || queued_message_.empty()) return pw::Status::Unavailable();
    rx_pending_ = false;
    std::string message = queued_message_;
    queued_message_.clear();
    return message;
}

pw::Result<bool> cap_lora::CapLoRa1262::msg_send(const std::string& payload)
{
    if (payload.empty() || payload.size() > kMaxTextPayload) return pw::Status::InvalidArgument();
    if (!initialized_) return pw::Status::Unavailable();
    if (!sx1262_ || tx_in_progress_) return pw::Status::FailedPrecondition();
    if (sx1262_->standby() != RADIOLIB_ERR_NONE) return pw::Status::Internal();
    received_flag_.store(false, std::memory_order_release);
    transmitted_flag_.store(false, std::memory_order_release);
    if (sx1262_->startTransmit(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()) != RADIOLIB_ERR_NONE) {
        (void)sx1262_->startReceive();
        return pw::Status::Internal();
    }
    tx_in_progress_ = true;
    tx_mode_ = true;
    has_sent_message_ = true;
    last_tx_ = payload;
    tx_pending_ = false;
    return true;
}

bool cap_lora::CapLoRa1262::send(const std::string& payload)
{
    const auto result = msg_send(payload);
    return result.ok() && *result;
}

void cap_lora::CapLoRa1262::request_stop() noexcept { stop_requested_.store(true, std::memory_order_release); }
void cap_lora::CapLoRa1262::clear_stop() noexcept { stop_requested_.store(false, std::memory_order_release); }

void cap_lora::CapLoRa1262::shutdown()
{
    initialized_ = false;
    tx_in_progress_ = false;
    tx_mode_ = false;
    rx_pending_ = false;
    tx_pending_ = false;
    queued_message_.clear();
    if (sx1262_) {
        (void)sx1262_->standby();
        (void)sx1262_->sleep();
    }
    sx1262_.reset();
    MyModule_.reset();
    cp0_lora_pi4io_controller::shutdown();
    cp0_lora_hat_power_controller::shutdown();
    cp0_lora_backend::gpio_release_all();
    pi4ioe5v6408_.reset();
    RadioHal_.reset();
    spi_initiator_.reset();
    gpio_chip_.reset();
    I2cInitiator_.reset();
}

bool cap_lora::CapLoRa1262::setup() { return initialize(); }
bool cap_lora::CapLoRa1262::StartServer() { return initialize(); }
void cap_lora::CapLoRa1262::StopServer() { shutdown(); }
bool cap_lora::CapLoRa1262::isReady() const { return initialized_; }
bool cap_lora::CapLoRa1262::exec_send(const char* payload) { return payload != nullptr && send(payload); }
void cap_lora::CapLoRa1262::exec_poll() { if (initialized_) (void)service_radio(); }
void cap_lora::CapLoRa1262::start_receive() { if (initialized_) (void)set_rx_mode(); }
void cap_lora::CapLoRa1262::set_tx_mode(bool enabled) { if (enabled) (void)set_tx_mode(); else (void)set_rx_mode(); }

void cap_lora::CapLoRa1262::get_info(LoraInfo* info, bool drain_events) const
{
    if (!info) return;
    *info = LoraInfo{};
    std::snprintf(info->spi_device, sizeof(info->spi_device), "%s", spi_device_.c_str());
    std::snprintf(info->diag, sizeof(info->diag), "%s", initialized_ ? "SX1262 ready" : "SX1262 not initialized");
    std::snprintf(info->pi4io_status, sizeof(info->pi4io_status), "%s", initialized_ ? "PI4IO ready" : "PI4IO not initialized");
    info->initialized = initialized_ ? 1 : 0;
    info->hw_ready = initialized_ ? 1 : 0;
    info->tx_mode = tx_mode_ ? 1 : 0;
    info->tx_in_progress = tx_in_progress_ ? 1 : 0;
    info->has_sent_message = has_sent_message_ ? 1 : 0;
    info->rx_event = rx_pending_ ? 1 : 0;
    info->tx_event = tx_pending_ ? 1 : 0;
    if (!queued_message_.empty()) {
        std::snprintf(info->last_rx, sizeof(info->last_rx), "%s", queued_message_.c_str());
        info->rssi = last_rssi_;
        info->snr = last_snr_;
    }
    if (!last_tx_.empty()) std::snprintf(info->last_tx, sizeof(info->last_tx), "%s", last_tx_.c_str());
    if (drain_events) {
        rx_pending_ = false;
        tx_pending_ = false;
        queued_message_.clear();
    }
}

// Amalgamated from cp0_lora_device_discovery.cpp.

#include <cstdio>
#include <cstring>

namespace cp0_lora_device_discovery {

size_t collect_spi_candidates(char out[][64], size_t max_count, const char* preferred)
{
    if (out == nullptr || max_count == 0) return 0;

    size_t count          = 0;
    auto append_candidate = [&](const char* path) {
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

}  // namespace cp0_lora_device_discovery

// GPIO access is intentionally expressed only through pw_digital_io_linux.
// The integer handles below are compatibility tokens for the existing
// RadioLib adapter; they never expose Linux GPIO file descriptors.
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace cp0_lora_backend {
namespace {
using Chip   = pw::digital_io::LinuxDigitalIoChip;
using Input  = pw::digital_io::LinuxDigitalIn;
using Output = pw::digital_io::LinuxDigitalOut;

std::mutex gpio_mutex;
std::map<std::string, std::shared_ptr<Chip>> chips;
std::map<int, std::shared_ptr<Input>> inputs;
std::map<int, std::shared_ptr<Output>> outputs;
std::map<int, std::shared_ptr<Output>> output_tokens;
int next_token = 1;

std::shared_ptr<Chip> get_chip(const char* path)
{
    const std::string key = path && path[0] ? path : "/dev/gpiochip0";
    const auto found      = chips.find(key);
    if (found != chips.end()) return found->second;
    auto opened = Chip::Open(key.c_str());
    if (!opened.ok()) return nullptr;
    auto chip = std::make_shared<Chip>(std::move(*opened));
    chips.emplace(key, chip);
    return chip;
}

bool resolve_line(const char* chip_env, const char* offset_env, int gpio, std::string* chip_path, uint32_t* offset)
{
    const auto resolution = cp0_lora_gpio_offset_policy::resolve(offset_env ? std::getenv(offset_env) : nullptr, gpio);
    if (!resolution.valid() || chip_path == nullptr || offset == nullptr) return false;
    *chip_path =
        chip_env && std::getenv(chip_env) && std::getenv(chip_env)[0] ? std::getenv(chip_env) : "/dev/gpiochip0";
    *offset = static_cast<uint32_t>(resolution.offset);
    return true;
}

int ensure_output(int gpio, const char* chip_env, const char* offset_env, int value)
{
    std::string path;
    uint32_t offset = 0;
    if (!resolve_line(chip_env, offset_env, gpio, &path, &offset)) return -1;
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto found = outputs.find(gpio);
    if (found != outputs.end())
        return found->second->SetState(value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive).ok()
                   ? 0
                   : -1;
    auto chip = get_chip(path.c_str());
    if (!chip) return -1;
    auto line = chip->GetOutputLine({offset, pw::digital_io::Polarity::kActiveHigh,
                                     value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive});
    if (!line.ok()) return -1;
    auto output = std::make_shared<Output>(std::move(*line));
    if (!output->Enable().ok() ||
        !output->SetState(value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive).ok())
        return -1;
    outputs.emplace(gpio, std::move(output));
    return 0;
}

int ensure_input(int gpio, const char* chip_env, const char* offset_env)
{
    std::string path;
    uint32_t offset = 0;
    if (!resolve_line(chip_env, offset_env, gpio, &path, &offset)) return -1;
    std::lock_guard<std::mutex> lock(gpio_mutex);
    if (inputs.find(gpio) != inputs.end()) return 0;
    auto chip = get_chip(path.c_str());
    if (!chip) return -1;
    auto line = chip->GetInputLine({offset, pw::digital_io::Polarity::kActiveHigh});
    if (!line.ok()) return -1;
    auto input = std::make_shared<Input>(std::move(*line));
    if (!input->Enable().ok()) return -1;
    inputs.emplace(gpio, std::move(input));
    return 0;
}
}  // namespace

int gpio_init_output(int gpio, int value)
{
    return ensure_output(gpio, nullptr, nullptr, value);
}

int gpio_get_value(int gpio)
{
    std::lock_guard<std::mutex> lock(gpio_mutex);
    const auto found = inputs.find(gpio);
    if (found == inputs.end()) return -1;
    const auto state = found->second->GetState();
    return state.ok() && *state == pw::digital_io::State::kActive ? 1 : (state.ok() ? 0 : -1);
}

bool gpio_open_output_line(const char* chip_path, int offset, int value, int* line_fd)
{
    if (line_fd == nullptr) return false;
    std::string path = chip_path && chip_path[0] ? chip_path : "/dev/gpiochip0";
    std::lock_guard<std::mutex> lock(gpio_mutex);
    auto chip = get_chip(path.c_str());
    if (!chip) return false;
    auto line = chip->GetOutputLine({static_cast<uint32_t>(offset), pw::digital_io::Polarity::kActiveHigh,
                                     value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive});
    if (!line.ok()) return false;
    auto output = std::make_shared<Output>(std::move(*line));
    if (!output->Enable().ok()) return false;
    const int token = next_token++;
    output_tokens.emplace(token, std::move(output));
    *line_fd = token;
    return true;
}

bool gpio_set_output_line_value(int line_fd, int value)
{
    std::lock_guard<std::mutex> lock(gpio_mutex);
    const auto found = output_tokens.find(line_fd);
    if (found == output_tokens.end()) return false;
    return found->second->SetState(value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive).ok();
}

int gpio_init_output_any(const char* chip_env_name, const char* offset_env_name, int gpio, int value, int* line_fd,
                         const char*)
{
    if (line_fd && *line_fd >= 0) return gpio_set_output_line_value(*line_fd, value) ? 0 : -1;
    return ensure_output(gpio, chip_env_name, offset_env_name, value);
}

int gpio_init_input_any(const char* chip_env_name, const char* offset_env_name, int gpio, int* line_fd, const char*)
{
    if (line_fd && *line_fd >= 0) return 0;
    const int result = ensure_input(gpio, chip_env_name, offset_env_name);
    if (line_fd) *line_fd = -1;
    return result;
}

int gpio_init_input_irq_any(const char* chip_env_name, const char* offset_env_name, int gpio, int* line_fd, const char*,
                            GpioIrqFdType* fd_type)
{
    if (fd_type) *fd_type = GpioIrqFdType::NONE;
    // Keep IRQ handling in the existing bounded radio poll loop. The line is
    // still opened by pw_digital_io_linux, so no raw Linux fd is required.
    return gpio_init_input_any(chip_env_name, offset_env_name, gpio, line_fd, "IRQ");
}

int gpio_get_value_any(int gpio, int)
{
    return gpio_get_value(gpio);
}

int gpio_set_value_any(int gpio, int, int value)
{
    std::lock_guard<std::mutex> lock(gpio_mutex);
    const auto found = outputs.find(gpio);
    if (found == outputs.end()) return -1;
    return found->second->SetState(value ? pw::digital_io::State::kActive : pw::digital_io::State::kInactive).ok() ? 0
                                                                                                                   : -1;
}

void gpio_release_all()
{
    std::lock_guard<std::mutex> lock(gpio_mutex);
    output_tokens.clear();
    outputs.clear();
    inputs.clear();
    chips.clear();
}

}  // namespace cp0_lora_backend

// Amalgamated from cp0_lora_hat_power_controller.cpp.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

#ifndef SLOGI
#define SLOGI(...)                  \
    do {                            \
        std::printf("[cap_lora] "); \
        std::printf(__VA_ARGS__);   \
        std::printf("\n");          \
    } while (0)
#endif

namespace cp0_lora_hat_power_controller {
namespace {

constexpr const char* LED_BRIGHTNESS_PATH = "/sys/class/leds/ext_5v_out/brightness";

enum class Backend {
    NONE,
    LED,
    GPIO,
};

std::mutex power_mutex;
Backend backend    = Backend::NONE;
int line_fd        = -1;
int line_offset    = 5;
char chip_path[64] = "";
std::string saved_brightness;

bool read_text_file(const char* path, std::string* value)
{
    if (path == nullptr || value == nullptr) return false;
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buffer[32] = {};
    ssize_t count;
    do {
        count = read(fd, buffer, sizeof(buffer) - 1);
    } while (count < 0 && errno == EINTR);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    if (count <= 0) return false;
    while (count > 0 && (buffer[count - 1] == '\n' || buffer[count - 1] == '\r')) --count;
    value->assign(buffer, static_cast<size_t>(count));
    return !value->empty();
}

bool write_text_file(const char* path, const char* value)
{
    if (path == nullptr || value == nullptr) return false;
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const size_t length = strlen(value);
    ssize_t count;
    do {
        count = write(fd, value, length);
    } while (count < 0 && errno == EINTR);
    const int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return count == static_cast<ssize_t>(length);
}

void log_result(const char* stage, bool gpio_ok)
{
    const char* chip = chip_path[0] ? chip_path : "default";
    SLOGI("5VDBG %s gpio=%s chip=%s[%d]", stage ? stage : "?", gpio_ok ? "ok" : "fail", chip, line_offset);
}

enum class PrepareResult {
    READY,
    UNAVAILABLE,
    INVALID_OFFSET,
};

PrepareResult prepare_line(bool explicit_override)
{
    const char* chip_env   = getenv("HAT_5VOUT_CHIP");
    const char* offset_env = getenv("HAT_5VOUT_OFFSET");
    if (!explicit_override || !chip_env || !chip_env[0] || !offset_env || !offset_env[0] ||
        strlen(chip_env) >= sizeof(chip_path))
        return PrepareResult::INVALID_OFFSET;
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, 5);
    if (!offset_resolution.valid()) return PrepareResult::INVALID_OFFSET;
    snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    line_offset = offset_resolution.offset;
    if (line_fd >= 0) return PrepareResult::READY;
    return cp0_lora_backend::gpio_open_output_line(chip_path, line_offset, 1, &line_fd) ? PrepareResult::READY
                                                                                        : PrepareResult::UNAVAILABLE;
}

}  // namespace

bool enable(int fallback_gpio)
{
    std::lock_guard<std::mutex> lock(power_mutex);
    (void)fallback_gpio;
    if (backend != Backend::NONE) return true;

    const bool explicit_override = getenv("HAT_5VOUT_CHIP") != nullptr || getenv("HAT_5VOUT_OFFSET") != nullptr;
    if (!explicit_override && read_text_file(LED_BRIGHTNESS_PATH, &saved_brightness)) {
        if (write_text_file(LED_BRIGHTNESS_PATH, "1")) {
            backend = Backend::LED;
            SLOGI("5VDBG enabled via %s (saved=%s)", LED_BRIGHTNESS_PATH, saved_brightness.c_str());
            usleep(50000);
            return true;
        }
        saved_brightness.clear();
        SLOGI("5VDBG write %s failed errno=%d", LED_BRIGHTNESS_PATH, errno);
        return false;
    }
    if (!explicit_override) {
        SLOGI("5VDBG %s unavailable; refusing implicit GPIO fallback", LED_BRIGHTNESS_PATH);
        return false;
    }

    const PrepareResult prepared = prepare_line(explicit_override);
    if (prepared == PrepareResult::INVALID_OFFSET) {
        SLOGI("5VDBG invalid/incomplete HAT_5VOUT_CHIP+HAT_5VOUT_OFFSET override");
        return false;
    }
    if (prepared == PrepareResult::READY && cp0_lora_backend::gpio_set_output_line_value(line_fd, 0)) {
        backend = Backend::GPIO;
        log_result("gpio_set", true);
        usleep(50000);
        return true;
    }
    if (line_fd >= 0) {
        line_fd = -1;
    }
    SLOGI("5VDBG no safe HAT power control path; refusing GPIO5 fallback");
    return false;
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(power_mutex);
    if (backend == Backend::LED) {
        if (!saved_brightness.empty() && !write_text_file(LED_BRIGHTNESS_PATH, saved_brightness.c_str()))
            SLOGI("5VDBG restore %s failed errno=%d", LED_BRIGHTNESS_PATH, errno);
    }
    if (line_fd >= 0) {
        if (!cp0_lora_backend::gpio_set_output_line_value(line_fd, 1))
            SLOGI("5VDBG disable gpio line failed errno=%d", errno);
        line_fd = -1;
    }
    backend = Backend::NONE;
    saved_brightness.clear();
    chip_path[0] = '\0';
    line_offset  = 5;
}

}  // namespace cp0_lora_hat_power_controller

// Amalgamated from cp0_lora_pi4io_controller.cpp.

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unistd.h>

#include "pw_i2c/initiator.h"
#include "pw_i2c_linux/initiator.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

#include "driver/PI4IO/PI4IOE5V6408_Class.hpp"

namespace cp0_lora_pi4io_controller {
namespace {

constexpr int I2C_BUS           = 1;
constexpr int SDA_GPIO          = 2;
constexpr int SCL_GPIO          = 3;
constexpr uint8_t I2C_ADDRESS   = 0x43;
constexpr auto OPERATION_BUDGET = std::chrono::milliseconds(1200);

char status_text[160]  = "I2C 0x43 not checked";
uint8_t output_cache   = 0x00;
uint8_t config_cache   = 0xFF;
uint8_t polarity_cache = 0x00;
std::mutex controller_mutex;
std::atomic<bool> stop_requested{false};

struct RegisterSnapshot {
    bool valid       = false;
    uint8_t output   = 0;
    uint8_t config   = 0;
    uint8_t polarity = 0;
};

RegisterSnapshot saved_registers;

using Deadline = std::chrono::steady_clock::time_point;

bool should_stop(const Deadline& deadline)
{
    return stop_requested.load(std::memory_order_acquire) || std::chrono::steady_clock::now() >= deadline;
}

// Bus sessions hand out a pw::i2c::Initiator so the register protocol runs
// through Pigweed's I2C stack (pw_i2c_linux on the device). The factory is a
// pointer so tests can substitute a fake initiator and drive the snapshot /
// restore / cancellation logic without any hardware; everything below the
// factory is pure protocol code.
using InitiatorPtr = std::unique_ptr<pw::i2c::Initiator>;
InitiatorPtr default_open_bus(const char* path);
InitiatorPtr (*open_bus_factory)(const char* path) = default_open_bus;

// pw_i2c_linux owns the bus descriptor and serializes transactions. The
// address is carried by each Initiator message, so no raw i2c-dev ioctl is
// needed here.
InitiatorPtr default_open_bus(const char* path)
{
    pw::Result<int> fd = pw::i2c::LinuxInitiator::OpenI2cBus(path);
    if (!fd.ok()) {
        snprintf(status_text, sizeof(status_text), "open %s failed (status=%d), SDA:%d SCL:%d", path,
                 static_cast<int>(fd.status().code()), SDA_GPIO, SCL_GPIO);
        return nullptr;
    }
    return std::make_unique<pw::i2c::LinuxInitiator>(*fd);
}

bool probe(m5::PI4IOE5V6408_Class& pi4io)
{
    // begin() reads the ID register: a failing transfer means the expander
    // did not acknowledge its address on the bus.
    if (!pi4io.begin()) {
        snprintf(status_text, sizeof(status_text), "I2C 0x%02X not found on /dev/i2c-%d (SDA:%d SCL:%d)", I2C_ADDRESS,
                 I2C_BUS, SDA_GPIO, SCL_GPIO);
        return false;
    }
    snprintf(status_text, sizeof(status_text), "I2C 0x%02X found on /dev/i2c-%d (SDA:%d SCL:%d)", I2C_ADDRESS, I2C_BUS,
             SDA_GPIO, SCL_GPIO);
    return true;
}

bool restore_registers(m5::PI4IOE5V6408_Class& pi4io, const RegisterSnapshot& snapshot)
{
    if (!snapshot.valid) return true;
    bool ok = true;
    const struct {
        uint8_t reg;
        uint8_t saved;
    } registers[] = {
        {0x01, snapshot.output},
        {0x02, snapshot.polarity},
        {0x03, snapshot.config},
    };
    for (const auto& item : registers) {
        const pw::Result<uint8_t> current = pi4io.readRegister8(item.reg);
        if (!current.ok()) {
            ok = false;
            continue;
        }
        // This controller owns only P0 in each register; leave the bits
        // other I2C clients may have changed while the app was running
        // untouched.
        const uint8_t restored = static_cast<uint8_t>((*current & ~uint8_t{0x01}) | (item.saved & uint8_t{0x01}));
        if (!pi4io.writeRegister8(item.reg, restored)) ok = false;
    }
    return ok;
}

bool initialize(m5::PI4IOE5V6408_Class& pi4io, const Deadline& deadline)
{
    RegisterSnapshot original;
    original.valid                     = true;
    const pw::Result<uint8_t> output   = pi4io.readRegister8(0x01);
    const pw::Result<uint8_t> polarity = pi4io.readRegister8(0x02);
    const pw::Result<uint8_t> config   = pi4io.readRegister8(0x03);
    if (should_stop(deadline) || !output.ok() || !polarity.ok() || !config.ok()) {
        const int code = !output.ok()     ? static_cast<int>(output.status().code())
                         : !polarity.ok() ? static_cast<int>(polarity.status().code())
                                          : static_cast<int>(config.status().code());
        snprintf(status_text, sizeof(status_text), "I2C IO snapshot failed at 0x%02X status=%d", I2C_ADDRESS, code);
        return false;
    }
    original.output   = *output;
    original.polarity = *polarity;
    original.config   = *config;

    polarity_cache = static_cast<uint8_t>(original.polarity & ~uint8_t{0x01});
    output_cache   = static_cast<uint8_t>(original.output | uint8_t{0x01});
    config_cache   = static_cast<uint8_t>(original.config & ~uint8_t{0x01});
    struct RegisterWrite {
        uint8_t reg;
        uint8_t value;
        const char* name;
    };
    const RegisterWrite writes[] = {
        {0x02, polarity_cache, "POL"},
        {0x01, output_cache, "OUT"},
        {0x03, config_cache, "CFG"},
    };
    for (const auto& item : writes) {
        if (should_stop(deadline)) {
            snprintf(status_text, sizeof(status_text), "I2C IO init cancelled");
            (void)restore_registers(pi4io, original);
            return false;
        }
        if (!pi4io.writeRegister8(item.reg, item.value)) {
            snprintf(status_text, sizeof(status_text), "I2C IO write %s failed at 0x%02X status=%d", item.name,
                     I2C_ADDRESS, static_cast<int>(pi4io.lastStatus().code()));
            (void)restore_registers(pi4io, original);
            return false;
        }
    }

    saved_registers = original;

    snprintf(status_text, sizeof(status_text), "I2C IO init ok OUT=0x%02X POL=0x%02X CFG=0x%02X P0=HIGH", output_cache,
             polarity_cache, config_cache);
    return true;
}

}  // namespace

bool scan_and_initialize()
{
    std::lock_guard<std::mutex> lock(controller_mutex);
    if (saved_registers.valid) return true;
    const Deadline deadline = std::chrono::steady_clock::now() + OPERATION_BUDGET;
    if (should_stop(deadline)) {
        snprintf(status_text, sizeof(status_text), "I2C IO init cancelled");
        return false;
    }
    InitiatorPtr initiator = open_bus_factory("/dev/i2c-1");
    if (!initiator) return false;
    // The initiator (and the exclusive bus lock it owns) is released when
    // this scope ends, after the last register transfer.
    m5::PI4IOE5V6408_Class pi4io(*initiator, I2C_ADDRESS);
    const bool ok = !should_stop(deadline) && probe(pi4io) && !should_stop(deadline) && initialize(pi4io, deadline);
    return ok;
}

void request_stop() noexcept
{
    stop_requested.store(true, std::memory_order_release);
}

void clear_stop() noexcept
{
    stop_requested.store(false, std::memory_order_release);
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(controller_mutex);
    if (!saved_registers.valid) return;
    InitiatorPtr initiator = open_bus_factory("/dev/i2c-1");
    if (!initiator) return;
    m5::PI4IOE5V6408_Class pi4io(*initiator, I2C_ADDRESS);
    const bool ok = restore_registers(pi4io, saved_registers);
    if (ok) {
        saved_registers = {};
        snprintf(status_text, sizeof(status_text), "I2C IO state restored");
    } else {
        snprintf(status_text, sizeof(status_text), "I2C IO restore failed at 0x%02X", I2C_ADDRESS);
    }
}

const char* status()
{
    return status_text;
}

}  // namespace cp0_lora_pi4io_controller
