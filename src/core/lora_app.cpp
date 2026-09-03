#include "core/lora_app.hpp"

#include <lvgl.h>
#include <spdlog/spdlog.h>

namespace cap_lora {

LoraApp::~LoraApp()
{
    stop();
}

void LoraApp::start()
{
    if (started_) return;
    started_ = true;
    quit_requested_ = false;
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0B0C0E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);
    setupInputGroup();
    screen_.onEnter(lv_screen_active());
    // A partially constructed LVGL tree cannot service input or timers.  Keep
    // the app marked started so stop() performs its normal cleanup, but ask
    // the main loop to terminate instead of spinning forever on a dead page.
    if (!screen_.active()) quit_requested_ = true;
}

void LoraApp::stop()
{
    if (!started_) return;
    screen_.onExit();
    if (input_group_) {
#if LV_USE_SDL
        lv_indev_t* indev = lv_indev_get_next(nullptr);
        while (indev) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD)
                lv_indev_remove_event_cb_with_user_data(indev, onKeyboardEvent, this);
            indev = lv_indev_get_next(indev);
        }
#endif
        lv_group_del(input_group_);
        input_group_ = nullptr;
    }
    started_ = false;
    spdlog::info("LoraApp: stop complete");
}

bool LoraApp::onLvglKeyState(uint32_t key, const char* utf8, bool pressed)
{
    if (!pressed) return true;
    try {
        if (key >= 0x20 && key <= 0x7e) {
            (void)utf8;
        }
        const bool handled = screen_.handleKey(key);
        if (key == LV_KEY_ESC && !screen_.active()) quit_requested_ = true;
        return handled;
    } catch (...) {
        // Keyboard callbacks run from both the evdev poll path and LVGL's SDL
        // event path. Do not let an unexpected UI exception unwind into either
        // loop; tear down the page and let main perform the regular shutdown.
        quit_requested_ = true;
        screen_.onExit();
        return true;
    }
}

void LoraApp::tick(uint32_t now_ms)
{
    screen_.tick(now_ms);
    if (started_ && !screen_.active()) quit_requested_ = true;
}

void LoraApp::onKeyboardEvent(lv_event_t* event)
{
    if (!event) return;
    auto* self = static_cast<LoraApp*>(lv_event_get_user_data(event));
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    if (!self || !indev || lv_indev_get_state(indev) != LV_INDEV_STATE_PRESSED) return;
    const uint32_t key = lv_indev_get_key(indev);
    char utf8[2] = {0, 0};
    if (key >= 0x20 && key < 0x7f) utf8[0] = static_cast<char>(key);
    (void)self->onLvglKeyState(key, utf8, true);
}

void LoraApp::setupInputGroup()
{
    if (input_group_) return;
    input_group_ = lv_group_create();
    if (!input_group_) return;
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            lv_indev_set_group(indev, input_group_);
#if LV_USE_SDL
            lv_indev_add_event_cb(indev, onKeyboardEvent, LV_EVENT_KEY, this);
#endif
        }
        indev = lv_indev_get_next(indev);
    }
}

}  // namespace cap_lora
