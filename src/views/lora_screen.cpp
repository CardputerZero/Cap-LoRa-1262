/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_screen.hpp"

#include "input/gps_keypad.hpp"
#include "models/lora_chat_protocol.hpp"
#include "models/lora_nickname_store.hpp"

#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>

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
constexpr uint32_t kClipboardNoticeMs = 1000;

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

void run_lora_initialization(const std::shared_ptr<lora_app_detail::LoraInitializationState> &state,
                             cap_lora::CapLoRa1262* device) noexcept
{
    int init_code    = -1;
    int info_code    = -1;
    int receive_code = -1;
    cap_lora::LoraInfo info{};

    try {
        const auto cancelled = [&] { return state->stop_requested.load(std::memory_order_acquire); };
        if (!cancelled()) {
            init_code = device && device->initialize() ? 0 : -1;
            if (!cancelled()) {
                if (device) device->get_info(&info, false);
                info_code = 0;
                if (!cancelled() && init_code == 0 && info.hw_ready) {
                    device->start_receive();
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
    model_.set_nickname(lora_nickname_store::load_or_default());
    lora_device_ = std::make_unique<cap_lora::CapLoRa1262>();
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
    if (!app_active_ && !poll_timer_ && !initialization_state_ &&
        !init_thread_.joinable() && !page_root_)
        return;
    spdlog::info("LoraScreen: onExit begin; cancelling animations and poll timer");
    cancel_view_animations();
    cancel_message_title_animation();
    clear_clipboard_notice();
    app_active_ = false;
    if (poll_timer_) { lv_timer_delete(poll_timer_); poll_timer_ = nullptr; }
    // Tell the worker to stop before reaping it. The worker only owns the
    // shared state, so it can safely observe this flag while the page is being
    // destroyed and will not start another hardware phase after cancellation.
    spdlog::info("LoraScreen: requesting radio worker stop");
    if (initialization_state_) initialization_state_->stop_requested.store(true, std::memory_order_release);
    if (lora_device_) lora_device_->request_stop();
    spdlog::info("LoraScreen: radio worker join begin (joinable={})", init_thread_.joinable());
    if (init_thread_.joinable()) init_thread_.join();
    spdlog::info("LoraScreen: radio worker join complete; radio shutdown begin");
    if (lora_device_) lora_device_->shutdown();
    spdlog::info("LoraScreen: radio shutdown complete; page deletion begin");
    initialization_state_.reset();
    initialization_pending_ = false;
    pending_tx_text_.clear();
    detach_delete_callbacks();
    if (page_root_) lv_obj_delete(page_root_);
    page_root_ = nullptr;
    root_screen_ = nullptr;
    active_view_ = nullptr;
    spdlog::info("LoraScreen: onExit complete");
}

void LoraScreen::tick(uint32_t)
{
}

bool LoraScreen::handleKey(uint32_t key)
{
    if (!app_active_) return false;
    if (help_view_ && !lv_obj_has_flag(help_view_, LV_OBJ_FLAG_HIDDEN)) {
        if (key == LV_KEY_ESC) hide_help();
        return true;
    }
    if (key == cap_gps::keyCommandValue(cap_gps::KeyCommand::Help)) {
        show_help();
        return true;
    }
    if (key == cap_gps::keyCommandValue(cap_gps::KeyCommand::Copy)) {
        copy_selected_message();
        return true;
    }
    if (key == cap_gps::keyCommandValue(cap_gps::KeyCommand::Paste)) {
        paste_clipboard();
        return true;
    }
    if (model_.editor_mode() == LoraEditorMode::NONE && model_.view() == LoraView::MESSAGES) {
        if (key == LV_KEY_ESC && model_.selected_message_index()) {
            clear_message_selection();
            return true;
        }
        if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
            select_message(key == LV_KEY_UP ? -1 : 1);
            return true;
        }
        if (model_.selected_message_index() && key == LV_KEY_ENTER) {
            open_reply_view();
            return true;
        }
        if (model_.selected_message_index()) {
            // A selected message is modal: arbitrary keys must not open a
            // fresh compose editor until the selection is cleared.
            return true;
        }
    }
    const LoraView input_view = model_.editor_mode() == LoraEditorMode::NONE ? model_.view() : LoraView::SEND;
    return handle_key(lora_app_detail::normalize_lora_key(key, input_view));
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
    if (lora_device_) lora_device_->clear_stop();

    initialization_pending_ = true;
    initialization_state_   = std::make_shared<lora_app_detail::LoraInitializationState>();
    const auto state        = initialization_state_;
    try {
        init_thread_ = std::thread([state, device = lora_device_.get()] {
            run_lora_initialization(state, device);
        });
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
    if (!lora_device_) return false;
    if (poll) lora_device_->exec_poll();
    lora_device_->get_info(&lora_info_, poll);
    return true;
}

void LoraScreen::append_chat_message(const char *text, bool outgoing, float rssi, float snr, std::string sender_name,
                                     LoraMessageDelivery delivery, std::string reply_to, std::string reply_to_sender)
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
    model_.append_message(text ? text : "", outgoing, rssi, snr, std::move(sender_name), delivery,
                          std::move(reply_to), std::move(reply_to_sender));
    last_message_row_ = append_message_row(model_.messages().back());
    set_visible(empty_message_label_, false);
    set_visible(empty_message_hint_label_, false);
    if (model_.view() == LoraView::MESSAGES && !model_.selected_message_index())
        scroll_to_latest(LV_ANIM_ON);
    else
        scroll_to_latest_pending_ = true;
}

void LoraScreen::rebuild_message_list()
{
    if (!message_list_) return;
    lv_obj_clean(message_list_);
    last_message_row_ = nullptr;
    for (size_t index = 0; index < model_.messages().size(); ++index)
        last_message_row_ = append_message_row(model_.messages()[index], model_.selected_message_index() == index);
    const bool empty = model_.messages().empty();
    set_visible(empty_message_label_, empty);
    set_visible(empty_message_hint_label_, empty);
    // Rebuilding clears and recreates every row, which resets the scroll
    // position to the top. Do not animate that internal repositioning: an
    // animated rebuild makes the whole history visibly slide from the first
    // message to the latest one after a send status update.
    if (!empty && model_.selected_message_index()) {
        lv_obj_t *selected_row = lv_obj_get_child(message_list_, static_cast<int32_t>(*model_.selected_message_index()));
        if (selected_row) lv_obj_scroll_to_view(selected_row, LV_ANIM_OFF);
    } else if (!empty) {
        scroll_to_latest(LV_ANIM_OFF);
    }
}

void LoraScreen::settle_pending_transmit()
{
    if (pending_tx_text_.empty()) return;
    bool completed = false;
    bool sent      = false;
    if (lora_info_.tx_event && std::strcmp(lora_info_.last_tx, pending_tx_text_.c_str()) == 0) {
        completed = true;
        sent      = true;
    } else if (!lora_info_.tx_in_progress) {
        completed = true;
    }
    if (!completed) return;
    if (model_.resolve_latest_pending(sent)) rebuild_message_list();
    pending_tx_text_.clear();
}

void LoraScreen::open_send_view(uint32_t first_key)
{
    model_.begin_send(lora_app_detail::key_to_ascii(first_key));
    render_current_view();
}

void LoraScreen::open_reply_view()
{
    const LoraChatMessage *message = model_.selected_message();
    if (!message) return;
    if (message->delivery != LoraMessageDelivery::RECEIVED && message->delivery != LoraMessageDelivery::SENT) {
        show_clipboard_notice(ClipboardNotice::Wait);
        return;
    }
    std::string sender = message->outgoing ? model_.nickname() : message->sender_name;
    if (sender.empty()) sender = "Unknown";
    model_.begin_reply(message->text, std::move(sender));
    render_current_view();
}

void LoraScreen::scroll_messages(int32_t amount)
{
    dismiss_message_title();
    if (message_list_) lv_obj_scroll_by_bounded(message_list_, 0, amount, LV_ANIM_ON);
}

void LoraScreen::select_message(int direction)
{
    dismiss_message_title();
    if (model_.select_message(direction)) rebuild_message_list();
}

void LoraScreen::copy_selected_message()
{
    if (model_.view() != LoraView::MESSAGES || model_.editor_mode() != LoraEditorMode::NONE) return;
    const LoraChatMessage *message = model_.selected_message();
    if (message) {
        clipboard_text_ = message->text;
        show_clipboard_notice(ClipboardNotice::Copied);
    }
}

void LoraScreen::paste_clipboard()
{
    if (model_.editor_mode() != LoraEditorMode::MESSAGE || clipboard_text_.empty()) return;
    clear_clipboard_notice();
    const size_t input_limit = model_.reply_to().empty() ? lora_chat_protocol::kMaxMessageBytes
                                                         : lora_chat_protocol::kMaxReplyMessageBytes;
    const size_t remaining = model_.tx_input().size() < input_limit ? input_limit - model_.tx_input().size() : 0;
    const bool truncated = clipboard_text_.size() > remaining;
    if (model_.insert_text(clipboard_text_))
        show_clipboard_notice(truncated ? ClipboardNotice::PastedTruncated : ClipboardNotice::Pasted);
    update_send_content();
}

void LoraScreen::show_clipboard_notice(ClipboardNotice notice)
{
    clear_clipboard_notice();
    clipboard_notice_ = notice;
    if (notice == ClipboardNotice::Copied || notice == ClipboardNotice::Wait) {
        show_message_notice(notice == ClipboardNotice::Copied ? "copied" : "wait");
    } else if (notice == ClipboardNotice::Pasted || notice == ClipboardNotice::PastedTruncated) {
        update_send_content();
    }
    clipboard_notice_timer_ =
        lv_timer_create(&LoraScreen::static_clipboard_notice_timer_cb, lora_app_detail::kClipboardNoticeMs, this);
    if (clipboard_notice_timer_)
        lv_timer_set_repeat_count(clipboard_notice_timer_, 1);
    else
        clear_clipboard_notice();
}

void LoraScreen::clear_clipboard_notice()
{
    if (clipboard_notice_timer_) {
        lv_timer_delete(clipboard_notice_timer_);
        clipboard_notice_timer_ = nullptr;
    }
    const ClipboardNotice notice = clipboard_notice_;
    clipboard_notice_ = ClipboardNotice::None;
    if (notice == ClipboardNotice::Copied || notice == ClipboardNotice::Wait) {
        hide_message_notice();
    } else if (notice == ClipboardNotice::Pasted || notice == ClipboardNotice::PastedTruncated) {
        update_send_content();
    }
}

void LoraScreen::clear_message_selection()
{
    if (model_.clear_message_selection()) rebuild_message_list();
}

void LoraScreen::show_help()
{
    if (!help_view_) return;
    clear_clipboard_notice();
    set_visible(help_view_, true);
    lv_obj_move_foreground(help_view_);
}

void LoraScreen::hide_help()
{
    set_visible(help_view_, false);
}

void LoraScreen::scroll_info(int32_t amount)
{
    if (info_table_) lv_obj_scroll_by_bounded(info_table_, 0, amount, LV_ANIM_ON);
}

void LoraScreen::open_nickname_editor()
{
    model_.begin_nickname_edit();
    render_current_view();
}

void LoraScreen::cancel_editor()
{
    clear_clipboard_notice();
    model_.cancel_editor();
    render_current_view();
}

void LoraScreen::save_nickname()
{
    const std::string nickname = model_.tx_input();
    if (nickname.empty() || nickname.find_first_not_of(' ') == std::string::npos) {
        model_.set_send_status("Nickname is empty");
        update_send_content();
        return;
    }
    std::string error;
    if (!lora_nickname_store::save(nickname, error)) {
        spdlog::error("LoRa nickname: {}", error);
        model_.set_send_status("Unable to save nickname");
        update_send_content();
        return;
    }
    model_.complete_nickname_edit(nickname);
    render_current_view();
}

bool LoraScreen::handle_send_key(uint32_t key)
{
    if (key == LV_KEY_ESC) {
        cancel_editor();
    } else if (key == LV_KEY_LEFT) {
        if (model_.move_cursor(-1)) update_send_content();
    } else if (key == LV_KEY_RIGHT) {
        if (model_.move_cursor(1)) update_send_content();
    } else if (key == LV_KEY_UP && model_.editor_mode() == LoraEditorMode::MESSAGE) {
        move_send_cursor_vertical(-1);
        update_send_content();
    } else if (key == LV_KEY_DOWN && model_.editor_mode() == LoraEditorMode::MESSAGE) {
        move_send_cursor_vertical(1);
        update_send_content();
    } else if (key == LV_KEY_END) {
        model_.set_cursor_position(model_.tx_input().size());
        update_send_content();
    } else if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        model_.erase_character();
        update_send_content();
    } else if (key == LV_KEY_ENTER) {
        if (model_.editor_mode() == LoraEditorMode::NICKNAME)
            save_nickname();
        else
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
    if (model_.view() == LoraView::INFO && (key == LV_KEY_UP || key == LV_KEY_DOWN)) {
        scroll_info(key == LV_KEY_UP ? lora_app_detail::kMessageScrollStep : -lora_app_detail::kMessageScrollStep);
        return true;
    }
    if (model_.view() == LoraView::INFO && key == LV_KEY_ENTER) {
        open_nickname_editor();
        return true;
    }
    if (model_.view() == LoraView::INFO && lora_app_detail::is_printable_ascii(key)) return true;
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
    if (model_.editor_mode() != LoraEditorMode::NONE) return handle_send_key(key);
    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        // LoraApp converts an unhandled exit key into a quit request. Avoid
        // joining the initialization worker from inside an input callback.
        return false;
    }
    return handle_navigation_key(key);
}

void LoraScreen::append_text_key(uint32_t key)
{
    model_.append_character(lora_app_detail::key_to_ascii(key));
}

void LoraScreen::send_current_text()
{
    clear_clipboard_notice();
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
        model_.set_send_status("Message is empty");
        update_send_content();
        return;
    }
    std::string sent_text(model_.tx_input());
    const std::string reply_to(model_.reply_to());
    const std::string reply_to_sender(model_.reply_to_sender());
    const std::string payload = lora_chat_protocol::encode(sent_text, model_.nickname(), reply_to);
    if (lora_device_ && !payload.empty() && lora_device_->exec_send(payload.c_str())) {
        pending_tx_text_ = payload;
        clear_message_selection();
        append_chat_message(sent_text.c_str(), true, 0.0f, 0.0f, {}, LoraMessageDelivery::PENDING,
                            reply_to, reply_to_sender);
        model_.complete_send();
        refresh_lora_info(false);
        render_current_view();
    } else {
        model_.set_send_status("Send failed");
        update_send_content();
    }
}

void LoraScreen::on_poll_timer()
{
    if (!app_active_ || !page_root_) return;
    if (model_.editor_mode() != LoraEditorMode::NONE) update_send_cursor();
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
    settle_pending_transmit();
    if (lora_info_.rx_event) {
        const auto received = lora_chat_protocol::decode(lora_info_.last_rx);
        std::string reply_to;
        std::string reply_to_sender;
        if (received.has_reply) {
            const LoraChatMessage *target = model_.find_message(received.reply_id);
            if (target) {
                reply_to = target->text;
                reply_to_sender = target->outgoing ? model_.nickname() : target->sender_name;
                if (reply_to_sender.empty()) reply_to_sender = "Unknown";
            } else {
                reply_to = "Original message unavailable";
                reply_to_sender = "Unknown";
            }
        }
        append_chat_message(received.message.c_str(), false, lora_info_.rssi, lora_info_.snr,
                            std::move(received.nickname), LoraMessageDelivery::RECEIVED,
                            std::move(reply_to), std::move(reply_to_sender));
    }
    if (model_.view() == LoraView::INFO) update_info_content();
}

void LoraScreen::static_cancel_button_cb(lv_event_t *event) noexcept
{
    LoraScreen *self = nullptr;
    try {
        if (!event) return;
        self = static_cast<LoraScreen *>(lv_event_get_user_data(event));
        if (!self || !lora_send_action_callback_allowed(lv_event_get_current_target(event), self->send_cancel_button_,
                                                        self->app_active_,
                                                        self->model_.editor_mode() != LoraEditorMode::NONE))
            return;
        self->cancel_editor();
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
                                                        self->app_active_,
                                                        self->model_.editor_mode() != LoraEditorMode::NONE))
            return;
        if (self->model_.editor_mode() == LoraEditorMode::NICKNAME)
            self->save_nickname();
        else
            self->send_current_text();
    } catch (...) {
        if (self) self->app_active_ = false;
    }
}

void LoraScreen::static_nickname_button_cb(lv_event_t *event) noexcept
{
    try {
        if (!event) return;
        auto *self = static_cast<LoraScreen *>(lv_event_get_user_data(event));
        if (!self || lv_event_get_current_target(event) != self->info_change_name_button_ || !self->app_active_ ||
            self->model_.view() != LoraView::INFO || self->model_.editor_mode() != LoraEditorMode::NONE)
            return;
        self->open_nickname_editor();
    } catch (...) {
        auto *self = event ? static_cast<LoraScreen *>(lv_event_get_user_data(event)) : nullptr;
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

void LoraScreen::static_clipboard_notice_timer_cb(lv_timer_t *timer) noexcept
{
    auto *self = timer ? static_cast<LoraScreen *>(lv_timer_get_user_data(timer)) : nullptr;
    if (!self || self->clipboard_notice_timer_ != timer) return;
    self->clipboard_notice_timer_ = nullptr;
    try {
        self->clear_clipboard_notice();
    } catch (...) {
        self->app_active_ = false;
    }
}
