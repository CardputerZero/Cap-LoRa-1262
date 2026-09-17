/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "models/lora_page_model.hpp"
#include "models/lora_page_contract.hpp"
#include "lora/cap_lora_1262.hpp"
#include <lvgl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace lora_app_detail {
struct LoraInitializationState;

// APPLaunch exposes F/X as navigation shortcuts outside the send editor.
// The keypad and SDL paths both deliver these keys as printable ASCII, so the
// view state must be considered before dispatching them to the page handler.
inline uint32_t normalize_lora_key(uint32_t key, LoraView view) noexcept
{
    if (view != LoraView::SEND) {
        if (key == 'f' || key == 'F') return LV_KEY_UP;
        if (key == 'x' || key == 'X') return LV_KEY_DOWN;
    }
    return key;
}

inline constexpr bool is_desktop_help_key(uint32_t key, LoraEditorMode editor_mode, bool help_visible,
                                          bool desktop) noexcept
{
    return desktop && (help_visible || editor_mode == LoraEditorMode::NONE) && (key == 'h' || key == 'H');
}
}  // namespace lora_app_detail

class LoraScreen {
public:
    LoraScreen();
    ~LoraScreen();
    LoraScreen(const LoraScreen &)            = delete;
    LoraScreen &operator=(const LoraScreen &) = delete;

    void onEnter(lv_obj_t *parent);
    void onExit();
    void tick(uint32_t now_ms);
    bool handleKey(uint32_t key);
    bool active() const noexcept
    {
        return app_active_;
    }

private:
    enum class ClipboardNotice : uint8_t { None, Copied, Pasted, PastedTruncated, Wait };

    enum class InfoRowId : uint8_t {
        Nickname,
        Rssi,
        Snr,
        Radio,
        Frequency,
        Version,
        Modulation,
        Bandwidth,
        SpreadingFactor,
        CodingRate,
        Power,
        Preamble,
        SyncWord,
        TcxoVoltage,
        CurrentLimit,
        PayloadLimit,
        SpiDevice,
        Pi4io,
        Count,
    };

    static constexpr std::size_t INFO_ROW_COUNT = static_cast<std::size_t>(InfoRowId::Count);

    LoraPageModel model_;
    bool app_active_               = false;
    bool initialization_pending_   = false;
    bool scroll_to_latest_pending_ = false;
    bool desktop_help_pressed_     = false;
    cap_lora::LoraInfo lora_info_{};
    std::shared_ptr<lora_app_detail::LoraInitializationState> initialization_state_;
    std::thread init_thread_;
    std::unique_ptr<cap_lora::CapLoRa1262> lora_device_;
    uint32_t last_init_attempt_tick_ = 0;
    std::string pending_tx_text_;
    std::string clipboard_text_;

    lv_timer_t *poll_timer_             = nullptr;
    lv_timer_t *message_title_timer_    = nullptr;
    lv_timer_t *clipboard_notice_timer_ = nullptr;
    ClipboardNotice clipboard_notice_   = ClipboardNotice::None;
    lv_obj_t *page_root_                = nullptr;
    lv_obj_t *help_view_                = nullptr;
    lv_obj_t *help_content_             = nullptr;
    lv_obj_t *messages_view_            = nullptr;
    lv_obj_t *message_list_             = nullptr;
    lv_obj_t *messages_title_           = nullptr;
    lv_obj_t *empty_message_label_      = nullptr;
    lv_obj_t *empty_message_hint_label_ = nullptr;
    lv_obj_t *last_message_row_         = nullptr;
    lv_obj_t *info_view_                = nullptr;
    lv_obj_t *info_status_dot_          = nullptr;
    lv_obj_t *info_status_label_        = nullptr;
    lv_obj_t *info_change_name_button_  = nullptr;
    lv_obj_t *info_stats_label_         = nullptr;
    lv_obj_t *info_table_               = nullptr;
    lv_obj_t *info_table_content_       = nullptr;
    std::array<lv_obj_t *, INFO_ROW_COUNT> info_value_labels_{};
    lv_obj_t *send_view_           = nullptr;
    lv_obj_t *send_title_label_    = nullptr;
    lv_obj_t *send_input_bubble_   = nullptr;
    lv_obj_t *send_input_label_    = nullptr;
    lv_obj_t *send_cursor_label_   = nullptr;
    lv_obj_t *send_status_label_   = nullptr;
    lv_obj_t *send_cancel_button_  = nullptr;
    lv_obj_t *send_confirm_button_ = nullptr;
    lv_obj_t *page_indicator_      = nullptr;
    lv_obj_t *page_dots_[2]        = {nullptr, nullptr};
    lv_obj_t *active_view_         = nullptr;
    lv_obj_t *root_screen_         = nullptr;

    static void set_visible(lv_obj_t *object, bool visible);

    static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                                lv_color_t color, lv_opa_t opacity, lv_coord_t radius);

    static lv_obj_t *make_plain_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                          lv_coord_t height);

    static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                lv_coord_t height, const lv_font_t *font, lv_color_t color, lv_text_align_t align);

    static lv_obj_t *make_divider(lv_obj_t *parent, lv_coord_t y);

    static void bubble_tail_draw_cb(lv_event_t *event) noexcept;

    void create_ui();
    void create_help_view();
    void create_messages_view();
    void create_info_view();
    void create_send_view();

    lv_obj_t *make_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, const char *text,
                                 lv_color_t background, lv_color_t foreground, lv_event_cb_t callback);
    void create_page_indicator();
    void track_owned_handle(lv_obj_t *object);
    bool ui_ready() const;
    void detach_delete_callbacks();
    void clear_deleted_handles(lv_obj_t *deleted);
    static void static_owned_obj_delete_cb(lv_event_t *event) noexcept;

    void init_lora();
    void start_lora_initialization();
    bool consume_lora_initialization();
    bool refresh_lora_info(bool poll);
    void update_page_indicator();
    void update_info_content();
    void update_send_content();
    void update_send_cursor();
    void configure_editor_layout();
    void move_send_cursor_vertical(int direction);
    void scroll_to_latest(lv_anim_enable_t animation);
    void schedule_message_title_dismissal();
    void dismiss_message_title();
    void cancel_message_title_animation();
    void show_message_notice(const char *text);
    void hide_message_notice();
    static void message_title_anim_exec_cb(void *object, int32_t y) noexcept;
    static void hide_message_title_after_anim_cb(lv_anim_t *animation) noexcept;
    static void view_opa_exec_cb(void *object, int32_t opacity) noexcept;
    static void hide_view_after_fade_cb(lv_anim_t *animation) noexcept;
    static void cancel_view_animation(lv_obj_t *view);
    void cancel_view_animations();
    static void animate_view_opacity(lv_obj_t *view, lv_opa_t start, lv_opa_t end, bool hide_after_fade);
    void transition_to_view(lv_obj_t *target);
    void render_current_view();

    lv_obj_t *append_message_row(const LoraChatMessage &message, bool selected = false);

    void append_chat_message(const char *text, bool outgoing, float rssi, float snr, std::string sender_name = {},
                             LoraMessageDelivery delivery = LoraMessageDelivery::RECEIVED, std::string reply_to = {},
                             std::string reply_to_sender = {});
    void rebuild_message_list();
    void settle_pending_transmit();
    void open_send_view(uint32_t first_key);
    void open_reply_view();
    void scroll_messages(int32_t amount);
    void select_message(int direction);
    void copy_selected_message();
    void paste_clipboard();
    void show_clipboard_notice(ClipboardNotice notice);
    void clear_clipboard_notice();
    void clear_message_selection();
    void show_help();
    void hide_help();
    void scroll_info(int32_t amount);
    void open_nickname_editor();
    void cancel_editor();
    void save_nickname();
    bool handle_send_key(uint32_t key);
    bool handle_navigation_key(uint32_t key);
    bool handle_key(uint32_t key);
    void append_text_key(uint32_t key);
    void send_current_text();
    void on_poll_timer();
    static void static_cancel_button_cb(lv_event_t *event) noexcept;
    static void static_send_button_cb(lv_event_t *event) noexcept;
    static void static_nickname_button_cb(lv_event_t *event) noexcept;
    static void static_poll_timer_cb(lv_timer_t *timer) noexcept;
    static void static_message_title_timer_cb(lv_timer_t *timer) noexcept;
    static void static_clipboard_notice_timer_cb(lv_timer_t *timer) noexcept;
};
