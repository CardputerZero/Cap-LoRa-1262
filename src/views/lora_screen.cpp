/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_screen.hpp"

#include <atomic>
#include <exception>
#include <mutex>
#include <thread>

namespace lora_app_detail {

struct LoraInitializationState {
    std::atomic<bool> stop_requested{false};
    std::mutex mutex;
    bool done        = false;
    int init_code    = -1;
    int info_code    = -1;
    int receive_code = -1;
    cap_lora::LoraInfo info{};
};

constexpr uint32_t kPollIntervalMs   = 300;
constexpr int32_t kMessageScrollStep = 36;
constexpr uint32_t kInitRetryIntervalMs = 3000;

static bool is_printable_ascii(uint32_t key)
{
    return key >= 0x20 && key <= 0x7e;
}

static char key_to_ascii(uint32_t key)
{
    return is_printable_ascii(key) ? static_cast<char>(key) : '\0';
}

static bool is_menu_prev_key(uint32_t key)
{
    return key == LV_KEY_LEFT || key == LV_KEY_PREV || key == 'z' || key == 'Z';
}

static bool is_menu_next_key(uint32_t key)
{
    return key == LV_KEY_RIGHT || key == LV_KEY_NEXT || key == 'c' || key == 'C';
}

}  // namespace lora_app_detail

namespace {

void run_lora_initialization(const std::shared_ptr<lora_app_detail::LoraInitializationState> &state) noexcept
{
    int init_code    = -1;
    int info_code    = -1;
    int receive_code = -1;
    cap_lora::LoraInfo info{};

    try {
        const auto cancelled = [&] { return state->stop_requested.load(std::memory_order_acquire); };
        if (!cancelled()) {
            init_code = cap_lora::backend::initialize() ? 0 : -1;
            if (!cancelled()) {
                cap_lora::backend::get_info(&info, false);
                info_code = 0;
                if (!cancelled() && init_code == 0 && info.hw_ready) {
                    cap_lora::backend::start_receive();
                    receive_code = 0;
                }
            }
        }
        if (cancelled()) {
            init_code = -1;
            std::snprintf(info.diag, sizeof(info.diag), "LoRa initialization cancelled");
        }
    } catch (...) {
        init_code = -1;
    }

    if (init_code != 0 && info.diag[0] == '\0')
        std::snprintf(info.diag, sizeof(info.diag), "LoRa initialization failed (rc=%d)", init_code);
    else if (info_code != 0 && info.diag[0] == '\0')
        std::snprintf(info.diag, sizeof(info.diag), "LoRa initialization response unavailable (rc=%d)", info_code);
    if (receive_code != 0 && info.hw_ready)
        std::snprintf(info.diag, sizeof(info.diag), "LoRa receive mode unavailable (rc=%d)", receive_code);

    std::lock_guard<std::mutex> lock(state->mutex);
    state->init_code    = init_code;
    state->info_code    = info_code;
    state->receive_code = receive_code;
    state->info         = info;
    state->done         = true;
}

}  // namespace

LoraScreen::LoraScreen()
{
}

LoraScreen::~LoraScreen()
{
    onExit();
}

void LoraScreen::onEnter(lv_obj_t* parent)
{
    onExit();
    root_screen_ = parent ? parent : lv_screen_active();
    create_ui();
    if (!ui_ready()) { onExit(); return; }
    init_lora();
}

void LoraScreen::onExit()
{
    cancel_view_animations();
    cancel_message_title_animation();
    app_active_ = false;
    if (poll_timer_) { lv_timer_delete(poll_timer_); poll_timer_ = nullptr; }
    // Tell the worker to stop before reaping it. The worker only owns the
    // shared state, so it can safely observe this flag while the page is being
    // destroyed and will not start another hardware phase after cancellation.
    if (initialization_state_) initialization_state_->stop_requested.store(true, std::memory_order_release);
    cap_lora::backend::request_stop();
    if (init_thread_.joinable()) init_thread_.join();
    cap_lora::backend::shutdown();
    initialization_state_.reset();
    initialization_pending_ = false;
    detach_delete_callbacks();
    if (page_root_) lv_obj_delete(page_root_);
    page_root_ = nullptr;
    root_screen_ = nullptr;
    active_view_ = nullptr;
}

void LoraScreen::tick(uint32_t)
{
}

bool LoraScreen::handleKey(uint32_t key)
{
    if (!app_active_) return false;
    return handle_key(key);
}

void LoraScreen::init_lora()
{
    if (!ui_ready()) return;
    app_active_               = true;
    scroll_to_latest_pending_ = false;
    lv_obj_clean(message_list_);
    last_message_row_ = nullptr;
    set_visible(empty_message_label_, true);
    set_visible(empty_message_hint_label_, true);
    lv_label_set_text(empty_message_hint_label_, "Initializing LoRa...");
    lv_obj_set_style_text_color(empty_message_hint_label_, lv_color_hex(0xC9A45C), LV_PART_MAIN | LV_STATE_DEFAULT);
    std::snprintf(lora_info_.diag, sizeof(lora_info_.diag), "Initializing LoRa hardware...");
    lora_info_.rx_event    = 0;
    lora_info_.tx_event    = 0;
    lora_info_.hw_ready    = 0;
    lora_info_.initialized = 0;
    model_.reset(false);
    render_current_view();
    poll_timer_ = lv_timer_create(&LoraScreen::static_poll_timer_cb, lora_app_detail::kPollIntervalMs, this);

    start_lora_initialization();
}

void LoraScreen::start_lora_initialization()
{
    // A previous attempt (successful or failed) must be fully reaped before a
    // new one starts, otherwise two init threads could race on the global
    // backend hardware state.
    if (initialization_state_) initialization_state_->stop_requested.store(true, std::memory_order_release);
    if (init_thread_.joinable()) init_thread_.join();
    cap_lora::backend::clear_stop();

    initialization_pending_ = true;
    initialization_state_   = std::make_shared<lora_app_detail::LoraInitializationState>();
    const auto state        = initialization_state_;
    try {
        init_thread_ = std::thread([state] { run_lora_initialization(state); });
    } catch (...) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->init_code    = -1;
        state->info_code    = -1;
        state->receive_code = -1;
        std::snprintf(state->info.diag, sizeof(state->info.diag), "Unable to start LoRa initialization");
        state->done = true;
    }
}

bool LoraScreen::consume_lora_initialization()
{
    if (!initialization_pending_ || !initialization_state_) return false;

    cap_lora::LoraInfo info{};
    {
        std::lock_guard<std::mutex> lock(initialization_state_->mutex);
        if (!initialization_state_->done) return false;
        info = initialization_state_->info;
    }

    initialization_pending_ = false;
    lora_info_              = info;
    lora_info_.rx_event     = 0;
    lora_info_.tx_event     = 0;
    model_.reset(lora_info_.hw_ready != 0);
    if (lora_info_.hw_ready) {
        lv_label_set_text(empty_message_hint_label_, "Type anything to send");
        lv_obj_set_style_text_color(empty_message_hint_label_, lv_color_hex(0x5FE492), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(empty_message_hint_label_, "LoRa unavailable; see Info");
        lv_obj_set_style_text_color(empty_message_hint_label_, lv_color_hex(0xD96C6C), LV_PART_MAIN | LV_STATE_DEFAULT);
        last_init_attempt_tick_ = lv_tick_get();
    }
    render_current_view();
    if (lora_info_.hw_ready) schedule_message_title_dismissal();
    return true;
}

bool LoraScreen::refresh_lora_info(bool poll)
{
    if (poll) cap_lora::backend::poll();
    cap_lora::backend::get_info(&lora_info_, poll);
    return true;
}

void LoraScreen::append_chat_message(const char *text, bool outgoing, float rssi, float snr)
{
    if (!message_list_) return;
    // A new bubble should never sit underneath the temporary Messages HUD.
    // The title is only an entry hint, so dismiss it as soon as real content
    // arrives instead of waiting for its timer.
    dismiss_message_title();
    const bool history_full = model_.messages().size() >= LoraPageModel::MESSAGE_HISTORY_LIMIT;
    if (history_full) {
        lv_obj_t *oldest_row = lv_obj_get_child(message_list_, 0);
        if (oldest_row) lv_obj_delete(oldest_row);
    }
    model_.append_message(text ? text : "", outgoing, rssi, snr);
    last_message_row_ = append_message_row(model_.messages().back());
    set_visible(empty_message_label_, false);
    set_visible(empty_message_hint_label_, false);
    if (model_.view() == LoraView::MESSAGES)
        scroll_to_latest(LV_ANIM_ON);
    else
        scroll_to_latest_pending_ = true;
}

void LoraScreen::open_send_view(uint32_t first_key)
{
    model_.begin_send(lora_app_detail::key_to_ascii(first_key));
    render_current_view();
}

void LoraScreen::scroll_messages(int32_t amount)
{
    dismiss_message_title();
    if (message_list_) lv_obj_scroll_by_bounded(message_list_, 0, amount, LV_ANIM_ON);
}

void LoraScreen::cancel_send()
{
    model_.cancel_send();
    render_current_view();
}

bool LoraScreen::handle_send_key(uint32_t key)
{
    if (key == LV_KEY_ESC) {
        cancel_send();
    } else if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        model_.erase_character();
        update_send_content();
    } else if (key == LV_KEY_ENTER) {
        send_current_text();
    } else if (lora_app_detail::is_printable_ascii(key)) {
        append_text_key(key);
        update_send_content();
    }
    return true;
}

bool LoraScreen::handle_navigation_key(uint32_t key)
{
    if (lora_app_detail::is_menu_prev_key(key)) {
        model_.set_view(LoraView::MESSAGES);
        render_current_view();
        return true;
    }
    if (lora_app_detail::is_menu_next_key(key)) {
        model_.set_view(LoraView::INFO);
        render_current_view();
        return true;
    }
    if (model_.view() == LoraView::MESSAGES && (key == LV_KEY_UP || key == LV_KEY_DOWN)) {
        scroll_messages(key == LV_KEY_UP ? lora_app_detail::kMessageScrollStep : -lora_app_detail::kMessageScrollStep);
        return true;
    }
    if (model_.view() != LoraView::MESSAGES && (key == LV_KEY_UP || key == LV_KEY_DOWN)) {
        model_.set_view(key == LV_KEY_UP ? LoraView::MESSAGES : LoraView::INFO);
        render_current_view();
        return true;
    }
    if (key == LV_KEY_ENTER) {
        if (initialization_pending_ || !lora_info_.hw_ready) return true;
        open_send_view(0);
        return true;
    }
    if (initialization_pending_ || !lora_info_.hw_ready) return lora_app_detail::is_printable_ascii(key);
    if (lora_app_detail::is_printable_ascii(key) && key != 'z' && key != 'Z' && key != 'c' && key != 'C') {
        open_send_view(key);
        return true;
    }
    return false;
}

bool LoraScreen::handle_key(uint32_t key)
{
    if (model_.view() == LoraView::SEND) return handle_send_key(key);
    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        onExit();
        return true;
    }
    return handle_navigation_key(key);
}

void LoraScreen::append_text_key(uint32_t key)
{
    model_.append_character(lora_app_detail::key_to_ascii(key));
}

void LoraScreen::send_current_text()
{
    if (initialization_pending_) {
        model_.set_send_status("LoRa is still initializing");
        update_send_content();
        return;
    }
    if (!lora_info_.hw_ready) {
        model_.set_send_status("LoRa unavailable");
        update_send_content();
        return;
    }
    if (model_.tx_input().empty()) {
        model_.set_send_status("Message is empty :(");
        update_send_content();
        return;
    }
    std::string sent_text(model_.tx_input());
    if (cap_lora::backend::send_text(sent_text.c_str())) {
        refresh_lora_info(false);
        append_chat_message(sent_text.c_str(), true, 0.0f, 0.0f);
        model_.complete_send();
        render_current_view();
    } else {
        model_.set_send_status("Send failed");
        update_send_content();
    }
}

void LoraScreen::on_poll_timer()
{
    if (!app_active_ || !page_root_) return;
    if (initialization_pending_) {
        (void)consume_lora_initialization();
        return;
    }
    if (!lora_info_.hw_ready) {
        // A transient init failure (cold boot, EXT5V rail still ramping, etc.)
        // must not wedge the page permanently: retry with a backoff so the
        // radio can come up on its own without hammering the backend.
        const uint32_t now = lv_tick_get();
        if (now - last_init_attempt_tick_ >= lora_app_detail::kInitRetryIntervalMs) {
            start_lora_initialization();
        }
        return;
    }
    if (!refresh_lora_info(true)) return;
    if (lora_info_.rx_event) append_chat_message(lora_info_.last_rx, false, lora_info_.rssi, lora_info_.snr);
    if (model_.view() == LoraView::INFO) update_info_content();
}

void LoraScreen::static_cancel_button_cb(lv_event_t *event) noexcept
{
    LoraScreen *self = nullptr;
    try {
        if (!event) return;
        self = static_cast<LoraScreen *>(lv_event_get_user_data(event));
        if (!self || !lora_send_action_callback_allowed(lv_event_get_current_target(event), self->send_cancel_button_,
                                                        self->app_active_, self->model_.view() == LoraView::SEND))
            return;
        self->cancel_send();
    } catch (...) {
        if (self) self->app_active_ = false;
    }
}

void LoraScreen::static_send_button_cb(lv_event_t *event) noexcept
{
    LoraScreen *self = nullptr;
    try {
        if (!event) return;
        self = static_cast<LoraScreen *>(lv_event_get_user_data(event));
        if (!self || !lora_send_action_callback_allowed(lv_event_get_current_target(event), self->send_confirm_button_,
                                                        self->app_active_, self->model_.view() == LoraView::SEND))
            return;
        self->send_current_text();
    } catch (...) {
        if (self) self->app_active_ = false;
    }
}

void LoraScreen::static_poll_timer_cb(lv_timer_t *timer) noexcept
{
    auto *self = static_cast<LoraScreen *>(lv_timer_get_user_data(timer));
    if (!self) return;
    try {
        if (lora_poll_callback_allowed(self->poll_timer_ == timer, self->app_active_)) self->on_poll_timer();
    } catch (...) {
        self->app_active_ = false;
    }
}

void LoraScreen::static_message_title_timer_cb(lv_timer_t *timer) noexcept
{
    auto *self = timer ? static_cast<LoraScreen *>(lv_timer_get_user_data(timer)) : nullptr;
    if (!self || self->message_title_timer_ != timer) return;
    self->message_title_timer_ = nullptr;
    try {
        self->dismiss_message_title();
    } catch (...) {
        self->app_active_ = false;
    }
}
