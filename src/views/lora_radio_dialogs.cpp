#include "views/lora_screen.hpp"

#include <spdlog/spdlog.h>

namespace {

constexpr uint32_t kAccent    = 0xFED40D;
constexpr uint32_t kText      = 0xF5F5F5;
constexpr uint32_t kSecondary = 0xB9B9B9;
constexpr uint32_t kError     = 0xFF9B91;

}  // namespace

void LoraScreen::create_radio_dialogs()
{
    radio_notice_decline_ = radio_notice_accept_ = radio_notice_skip_ = nullptr;
    radio_setup_status_ = radio_setup_close_ = radio_setup_save_ = nullptr;
    radio_region_label_ = radio_bandwidth_label_ = nullptr;
    radio_values_.fill(nullptr);
    radio_arrows_.fill(nullptr);
    const auto make_overlay = [this](lv_obj_t *&overlay) {
        overlay = make_panel(page_root_, 0, 0, lv_pct(100), lv_pct(100), lv_color_black(), LV_OPA_80, 0);
        if (!overlay) return static_cast<lv_obj_t *>(nullptr);
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        set_visible(overlay, false);
        track_owned_handle(overlay);
        lv_obj_t *panel = make_panel(overlay, 0, 0, 292, 150, lv_color_hex(0x141414), LV_OPA_COVER, 10);
        if (panel) {
            lv_obj_center(panel);
            lv_obj_set_style_outline_width(panel, 1, LV_PART_MAIN);
            lv_obj_set_style_outline_color(panel, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
        }
        return panel;
    };
    const auto label = [](lv_obj_t *parent, const char *text, int x, int y, int width, int height,
                          const lv_font_t *font, uint32_t color) {
        return make_label(parent, text, x, y, width, height, font, lv_color_hex(color), LV_TEXT_ALIGN_LEFT);
    };
    const auto primary_button = [this](lv_obj_t *panel, const char *text) {
        return make_action_button(panel, 151, 112, 125, text, lv_color_hex(kAccent), lv_color_hex(0x141414),
                                  radio_dialog_click_cb);
    };

    if (lv_obj_t *panel = make_overlay(radio_notice_)) {
        label(panel, "Radio regulations", 16, 12, 260, 22, &lv_font_montserrat_18, kAccent);
        label(panel, "Follow local radio regulations. Use only locally permitted frequencies and transmit power.", 16,
              38, 260, 51, &lv_font_montserrat_14, kText);

        radio_notice_skip_ = lv_checkbox_create(panel);
        if (auto *checkbox = radio_notice_skip_) {
            constexpr lv_style_selector_t checked =
                static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED;
            lv_checkbox_set_text(checkbox, "Don't show again");
            lv_obj_set_pos(checkbox, 16, 91);
            lv_obj_set_style_text_font(checkbox, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(checkbox, lv_color_hex(kText), LV_PART_MAIN);
            lv_obj_set_style_pad_column(checkbox, 8, LV_PART_MAIN);
            lv_obj_set_style_text_font(checkbox, &lv_font_montserrat_14, LV_PART_INDICATOR);
            lv_obj_set_style_pad_all(checkbox, 1, LV_PART_INDICATOR);
            lv_obj_set_style_radius(checkbox, 3, LV_PART_INDICATOR);
            lv_obj_set_style_border_color(checkbox, lv_color_hex(0x707070), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(checkbox, lv_color_hex(0x202020), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(checkbox, lv_color_hex(kAccent), checked);
            lv_obj_set_style_border_color(checkbox, lv_color_hex(kAccent), checked);
            lv_obj_set_style_text_color(checkbox, lv_color_black(), checked);
        }
        label(panel, "7 / Space", 202, 94, 74, 16, &lv_font_montserrat_12, kSecondary);
        radio_notice_decline_ = make_action_button(panel, 16, 109, 125, "Decline", lv_color_hex(0x292929),
                                                   lv_color_hex(kText), radio_dialog_click_cb);
        radio_notice_accept_  = make_action_button(panel, 151, 109, 125, "Accept", lv_color_hex(0x292929),
                                                   lv_color_hex(kText), radio_dialog_click_cb);
        make_label(panel, "Z / C: Choose   Enter: Confirm", 16, 137, 260, 13, &lv_font_montserrat_10,
                   lv_color_hex(kSecondary), LV_TEXT_ALIGN_CENTER);
    }

    if (lv_obj_t *panel = make_overlay(radio_setup_)) {
        label(panel, "Radio region", 16, 9, 260, 22, &lv_font_montserrat_18, kAccent);
        if (auto *selector = make_panel(panel, 16, 35, 260, 26, lv_color_hex(0x242424), LV_OPA_COVER, 4)) {
            lv_obj_set_style_outline_color(selector, lv_color_hex(kAccent), LV_PART_MAIN);
            lv_obj_set_style_outline_width(selector, 1, LV_PART_MAIN);
            radio_region_label_ = make_label(selector, "", 35, 4, 190, 18, &lv_font_montserrat_14, lv_color_hex(kText),
                                             LV_TEXT_ALIGN_CENTER);
            constexpr const char *arrows[] = {LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT};
            for (std::size_t direction = 0; direction < radio_arrows_.size(); ++direction) {
                auto *button = make_action_button(selector, direction == 0 ? 3 : 231, 3, 26, arrows[direction],
                                                  lv_color_hex(0x363636), lv_color_hex(kText), radio_dialog_click_cb);
                radio_arrows_[direction] = button;
                if (button) {
                    lv_obj_set_height(button, 20);
                    if (auto *icon = lv_obj_get_child(button, 0)) lv_obj_center(icon);
                }
            }
        }
        constexpr const char *captions[] = {"Frequency", "TX power", "Duty cycle"};
        constexpr int positions[]        = {16, 123, 212};
        constexpr int widths[]           = {105, 86, 64};
        for (std::size_t i = 0; i < radio_values_.size(); ++i) {
            label(panel, captions[i], positions[i], 68, widths[i], 14, &lv_font_montserrat_10, kSecondary);
            radio_values_[i] = label(panel, "", positions[i], 80, widths[i], 17, &lv_font_montserrat_14, kText);
        }
        radio_setup_status_    = label(panel, "", 16, 98, 201, 14, &lv_font_montserrat_10, kSecondary);
        radio_bandwidth_label_ = make_label(panel, "", 218, 98, 58, 14, &lv_font_montserrat_10,
                                            lv_color_hex(kSecondary), LV_TEXT_ALIGN_RIGHT);
        radio_setup_close_     = make_action_button(panel, 16, 112, 114, "Esc: Skip", lv_color_hex(0x292929),
                                                    lv_color_hex(kText), radio_dialog_click_cb);
        radio_setup_save_      = primary_button(panel, "Enter: Save");
    }
}

void LoraScreen::show_radio_setup(bool initial)
{
    if (!radio_notice_acknowledged_) return;
    radio_draft_  = radio_preferences_.settings;
    radio_dialog_ = RadioDialog::Setup;
    lv_label_set_text(lv_obj_get_child(radio_setup_close_, 0), initial ? "Esc: Skip" : "Esc: Cancel");
    update_radio_region();
    set_visible(radio_setup_, true);
    lv_obj_move_foreground(radio_setup_);
}

void LoraScreen::update_radio_region(const char *status)
{
    const auto *region = radio_draft_.preset();
    lv_label_set_text(radio_region_label_, region ? region->code.data() : "Unset");
    const char *values[] = {"--", "--", "--"};
    char frequency[24], power[16], duty[12], bandwidth[16];
    if (region) {
        std::snprintf(frequency, sizeof(frequency), "%.3f MHz", region->frequency_mhz());
        std::snprintf(power, sizeof(power), "%d dBm", region->tx_power_dbm());
        std::snprintf(duty, sizeof(duty), "%d%%", region->duty_percent);
        std::snprintf(bandwidth, sizeof(bandwidth), "%g kHz", region->bandwidth_khz());
        values[0] = frequency;
        values[1] = power;
        values[2] = duty;
    }
    for (std::size_t i = 0; i < radio_values_.size(); ++i) lv_label_set_text(radio_values_[i], values[i]);
    lv_label_set_text(radio_bandwidth_label_, region ? bandwidth : "");
    lv_obj_set_style_text_color(radio_setup_status_, lv_color_hex(status ? kError : kSecondary), LV_PART_MAIN);
    lv_label_set_text(radio_setup_status_, status ? status : "Z / C: Change region");
}

void LoraScreen::step_radio_region(bool forward)
{
    radio_draft_ = cap_lora::step_radio_region(radio_draft_, forward);
    update_radio_region();
}

bool LoraScreen::handle_radio_dialog_key(uint32_t key)
{
    if (radio_dialog_ == RadioDialog::Notice) {
        if (key == '7' || key == ' ') {
            if (lv_obj_has_state(radio_notice_skip_, LV_STATE_CHECKED))
                lv_obj_remove_state(radio_notice_skip_, LV_STATE_CHECKED);
            else
                lv_obj_add_state(radio_notice_skip_, LV_STATE_CHECKED);
        } else if (key == LV_KEY_LEFT || key == LV_KEY_PREV || key == 'z' || key == 'Z') {
            select_radio_notice_action(false);
        } else if (key == LV_KEY_RIGHT || key == LV_KEY_NEXT || key == 'c' || key == 'C') {
            select_radio_notice_action(true);
        } else if (key == LV_KEY_ENTER || key == '8') {
            if (radio_accept_selected_)
                accept_radio_notice();
            else
                exit_requested_ = true;
        } else if (key == LV_KEY_ESC) {
            exit_requested_ = true;
        }
        return true;
    }

    if (key == LV_KEY_ESC) {
        close_radio_setup();
        return true;
    }
    if (key == LV_KEY_ENTER) {
        save_radio_settings();
        return true;
    }
    if (key == LV_KEY_LEFT || key == 'z' || key == 'Z') {
        step_radio_region(false);
    } else if (key == LV_KEY_RIGHT || key == 'c' || key == 'C') {
        step_radio_region(true);
    }
    return true;
}

void LoraScreen::select_radio_notice_action(bool accept)
{
    radio_accept_selected_ = accept;
    const auto style       = [](lv_obj_t *button, bool selected) {
        lv_obj_set_style_bg_color(button, lv_color_hex(selected ? kAccent : 0x292929), LV_PART_MAIN);
        if (auto *label = lv_obj_get_child(button, 0))
            lv_obj_set_style_text_color(label, lv_color_hex(selected ? 0x141414 : kText), LV_PART_MAIN);
    };
    style(radio_notice_decline_, !accept);
    style(radio_notice_accept_, accept);
}

void LoraScreen::accept_radio_notice()
{
    radio_notice_acknowledged_ = true;
    if (lv_obj_has_state(radio_notice_skip_, LV_STATE_CHECKED)) {
        radio_preferences_.skip_notice = true;
        std::string error;
        if (!cap_lora::save_radio_preferences(radio_preferences_, error)) {
            spdlog::error("LoRa radio preferences: {}", error);
        }
    }
    set_visible(radio_notice_, false);
    radio_dialog_ = RadioDialog::None;
    if (radio_preferences_.settings.complete())
        start_lora_initialization();
    else
        show_radio_setup(true);
}

void LoraScreen::save_radio_settings()
{
    if (!radio_draft_.complete()) {
        update_radio_region("Select a region to use LoRa.");
        return;
    }

    const auto &current = radio_preferences_.settings;
    if (radio_draft_.region == current.region) {
        close_radio_setup();
        return;
    }
    if (initialization_pending_ || !pending_tx_text_.empty()) {
        update_radio_region(initialization_pending_ ? "Wait for LoRa to finish initializing."
                                                    : "Wait for your message to finish sending.");
        return;
    }

    auto preferences     = radio_preferences_;
    preferences.settings = radio_draft_;
    std::string error;
    if (!cap_lora::save_radio_preferences(preferences, error)) {
        spdlog::error("LoRa radio preferences: {}", error);
        update_radio_region("Could not save settings. Try again.");
        return;
    }
    radio_preferences_ = preferences;
    close_radio_setup();
    start_lora_initialization();
}

void LoraScreen::close_radio_setup()
{
    set_visible(radio_setup_, false);
    radio_dialog_ = RadioDialog::None;
}

void LoraScreen::radio_dialog_click_cb(lv_event_t *event) noexcept
{
    auto *self = static_cast<LoraScreen *>(lv_event_get_user_data(event));
    if (!self || !self->app_active_ || self->exit_requested_) return;
    auto *target = lv_event_get_current_target_obj(event);
    try {
        switch (self->radio_dialog_) {
            case RadioDialog::Notice:
                if (target == self->radio_notice_accept_)
                    self->accept_radio_notice();
                else if (target == self->radio_notice_decline_)
                    self->exit_requested_ = true;
                break;
            case RadioDialog::Setup:
                if (target == self->radio_setup_save_)
                    self->save_radio_settings();
                else if (target == self->radio_setup_close_)
                    self->close_radio_setup();
                else if (target == self->radio_arrows_[0] || target == self->radio_arrows_[1])
                    self->step_radio_region(target == self->radio_arrows_[1]);
                break;
            case RadioDialog::None:
                if (target == self->info_radio_settings_ && self->model_.view() == LoraView::INFO &&
                    self->model_.editor_mode() == LoraEditorMode::NONE) {
                    self->show_radio_setup(false);
                }
                break;
        }
    } catch (...) {
        self->app_active_ = false;
    }
}
