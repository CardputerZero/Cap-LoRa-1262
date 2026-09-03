#pragma once

#include "hal/gps_lvgl_hal.hpp"
#include "input/gps_keypad.hpp"
#include "views/lora_screen.hpp"

#include <cstdint>

namespace cap_lora {

class LoraApp {
public:
    LoraApp() = default;
    ~LoraApp();
    LoraApp(const LoraApp&) = delete;
    LoraApp& operator=(const LoraApp&) = delete;

    void start();
    void stop();
    bool onLvglKeyState(uint32_t key, const char* utf8, bool pressed);
    void tick(uint32_t now_ms);
    bool quitRequested() const { return quit_requested_; }

private:
    LoraScreen screen_;
    lv_group_t* input_group_ = nullptr;
    bool started_ = false;
    bool quit_requested_ = false;

    static void onKeyboardEvent(lv_event_t* event);
    void setupInputGroup();
};

}  // namespace cap_lora
