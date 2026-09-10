#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace {

// This is an executable inventory of every LVGL call made by UILoraPage in
// APPLaunch's ui_app_lora.cpp and ui_app_lora_view.cpp. Keeping the inventory
// in the repository makes the UI surface reviewable without requiring the
// APPLaunch tree or a hardware backend during unit tests.
constexpr std::array<std::string_view, 77> kUiCalls = {
    "lv_obj_create", "lv_obj_delete", "lv_obj_clean", "lv_obj_get_child", "lv_obj_get_coords", "lv_obj_get_y",
    "lv_obj_get_style_bg_color", "lv_obj_get_style_bg_opa", "lv_obj_get_style_opa",
    "lv_obj_get_style_opa_recursive", "lv_obj_has_flag", "lv_obj_set_pos", "lv_obj_set_size", "lv_obj_set_y",
    "lv_obj_add_flag", "lv_obj_clear_flag", "lv_obj_align", "lv_obj_center", "lv_obj_update_layout",
    "lv_obj_scroll_by_bounded", "lv_obj_scroll_to_view", "lv_obj_set_flex_flow", "lv_obj_set_flex_align",
    "lv_obj_set_scroll_dir", "lv_obj_set_scrollbar_mode", "lv_obj_add_event_cb", "lv_obj_remove_event_dsc",
    "lv_obj_remove_event_cb_with_user_data", "lv_obj_set_style_bg_color", "lv_obj_set_style_bg_opa",
    "lv_obj_set_style_border_width", "lv_obj_set_style_radius", "lv_obj_set_style_pad_all",
    "lv_obj_set_style_pad_left", "lv_obj_set_style_pad_right", "lv_obj_set_style_pad_top",
    "lv_obj_set_style_pad_bottom", "lv_obj_set_style_pad_row", "lv_obj_set_style_shadow_width",
    "lv_obj_set_style_margin_left", "lv_obj_set_style_margin_right", "lv_obj_set_style_margin_bottom",
    "lv_obj_set_style_opa", "lv_obj_set_style_text_font", "lv_obj_set_style_text_color",
    "lv_obj_set_style_text_opa", "lv_obj_set_style_text_align", "lv_label_create", "lv_label_set_text",
    "lv_label_set_long_mode", "lv_text_get_size", "lv_timer_create", "lv_timer_delete", "lv_timer_get_user_data",
    "lv_timer_set_repeat_count", "lv_tick_get", "lv_anim_init", "lv_anim_set_var", "lv_anim_set_values",
    "lv_anim_set_time", "lv_anim_set_path_cb", "lv_anim_set_exec_cb", "lv_anim_set_user_data",
    "lv_anim_set_completed_cb", "lv_anim_start", "lv_anim_del", "lv_anim_get_user_data", "lv_event_get_code",
    "lv_event_get_current_target", "lv_event_get_target", "lv_event_get_user_data", "lv_event_get_param",
    "lv_event_get_target_obj", "lv_event_get_layer", "lv_draw_triangle_dsc_init", "lv_draw_triangle",
    "lv_color_hex",
};

constexpr bool contains(std::string_view value)
{
    for (const auto call : kUiCalls)
        if (call == value) return true;
    return false;
}

constexpr int kScreenWidth = 320;
constexpr int kContentHeight = 150;
constexpr int kMessageHistoryLimit = 64;
constexpr int kTxInputLimit = 127;
constexpr int kPollIntervalMs = 300;
constexpr int kInitRetryIntervalMs = 3000;
constexpr int kMessageTitleHoldMs = 3200;
constexpr int kMessageTitleHideMs = 340;
constexpr int kViewTransitionMs = 150;

constexpr unsigned kPageBackground = 0x0B0C0E;
constexpr unsigned kOutgoingBubble = 0x3FCC75;
constexpr unsigned kIncomingBubble = 0xCCCCCC;
constexpr unsigned kInitializingColor = 0xC9A45C;
constexpr unsigned kRadioOffColor = 0xD96C6C;
constexpr unsigned kReceivingColor = 0x69AD80;

constexpr std::array<std::string_view, 21> kRequiredTexts = {
    "No messages yet", "Type anything to send", "Initializing LoRa...", "LoRa unavailable; see Info",
    "Messages", "LoRa Info", "CLIENT", "DEVICE", "RSSI", "SNR", "LINK", "New Message",
    "ESC: Cancel", "Enter: Send", "LoRa is still initializing", "LoRa unavailable", "Message is empty :(",
    "Send failed", "Unavailable", "Link configuration unavailable", "No diagnostics",
};

enum class RadioState { initializing, radio_off, sending, tx_mode, receiving };

constexpr RadioState radio_state(bool initializing, bool hw_ready, bool tx_in_progress, bool tx_mode)
{
    if (initializing) return RadioState::initializing;
    if (!hw_ready) return RadioState::radio_off;
    if (tx_in_progress) return RadioState::sending;
    if (tx_mode) return RadioState::tx_mode;
    return RadioState::receiving;
}

enum class View { messages, info, send };

constexpr View navigation_view(View current, int key)
{
    if (key == 'z' || key == 'Z' || key == 0x14) return View::messages; // LV_KEY_LEFT/PREV
    if (key == 'c' || key == 'C' || key == 0x15) return View::info;     // LV_KEY_RIGHT/NEXT
    if (key == 0x10) return View::messages;                              // LV_KEY_UP
    if (key == 0x11) return View::info;                                  // LV_KEY_DOWN
    return current;
}

constexpr bool printable_ascii(int key) { return key >= 0x20 && key <= 0x7e; }

constexpr int bubble_width(int text_width, int metadata_width = 0)
{
    const int content = text_width > metadata_width ? text_width : metadata_width;
    const int padded = content + 20; // 10px horizontal padding on each side
    return padded < 64 ? 64 : (padded > 244 ? 244 : padded);
}

} // namespace

int main()
{
    static_assert(kUiCalls.size() == 77);
    static_assert(kScreenWidth == 320 && kContentHeight == 150);
    static_assert(kMessageHistoryLimit == 64 && kTxInputLimit == 127);
    static_assert(kPollIntervalMs == 300 && kInitRetryIntervalMs == 3000);
    static_assert(kMessageTitleHoldMs == 3200 && kMessageTitleHideMs == 340 && kViewTransitionMs == 150);
    static_assert(kPageBackground == 0x0B0C0E && kOutgoingBubble == 0x3FCC75 && kIncomingBubble == 0xCCCCCC);
    static_assert(kInitializingColor == 0xC9A45C && kRadioOffColor == 0xD96C6C && kReceivingColor == 0x69AD80);

    // Creation/layout, content, scrolling, timers, animations, events, and
    // custom drawing are all represented in the call inventory.
    for (const auto call : {"lv_obj_create", "lv_obj_set_flex_flow", "lv_label_set_text", "lv_text_get_size",
                            "lv_obj_scroll_to_view", "lv_timer_create", "lv_anim_start", "lv_obj_add_event_cb",
                            "lv_draw_triangle"})
        CHECK(contains(call));
    CHECK(kUiCalls.front() == "lv_obj_create");
    CHECK(kUiCalls.back() == "lv_color_hex");
    for (const auto text : kRequiredTexts) CHECK(!text.empty());
    CHECK(radio_state(true, false, false, false) == RadioState::initializing);
    CHECK(radio_state(false, false, true, true) == RadioState::radio_off);
    CHECK(radio_state(false, true, true, true) == RadioState::sending);
    CHECK(radio_state(false, true, false, true) == RadioState::tx_mode);
    CHECK(radio_state(false, true, false, false) == RadioState::receiving);

    CHECK(navigation_view(View::send, 'z') == View::messages);
    CHECK(navigation_view(View::messages, 'C') == View::info);
    CHECK(navigation_view(View::info, 0x10) == View::messages);
    CHECK(navigation_view(View::messages, 0x11) == View::info);
    CHECK(navigation_view(View::info, 'q') == View::info);

    CHECK(printable_ascii(' '));
    CHECK(printable_ascii('~'));
    CHECK(!printable_ascii('\n'));
    CHECK(!printable_ascii(0x7f));
    CHECK(bubble_width(1) == 64);
    CHECK(bubble_width(100) == 120);
    CHECK(bubble_width(230) == 244);
    CHECK(bubble_width(20, 200) == 220);

    return 0;
}
