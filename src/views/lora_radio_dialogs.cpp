#include "views/lora_screen.hpp"

#include <spdlog/spdlog.h>

namespace {

constexpr std::size_t kFrequency = 0;
constexpr std::size_t kPower     = 1;
constexpr uint32_t kAccent       = 0xFED40D;
constexpr uint32_t kText         = 0xF5F5F5;
constexpr uint32_t kSecondary    = 0xB9B9B9;
constexpr uint32_t kError        = 0xFF9B91;

}  // namespace

void LoraScreen::create_radio_dialogs()
{
    radio_notice_ok_ = radio_notice_skip_ = radio_setup_status_ = radio_setup_close_ = radio_setup_save_ = nullptr;
    radio_rows_.fill(nullptr);
    radio_values_.fill(nullptr);
    radio_arrows_           = {};
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
        label(panel, "Esc: Exit", 16, 117, 132, 19, &lv_font_montserrat_12, kSecondary);
        radio_notice_ok_ = primary_button(panel, "Enter: Continue");
    }

    if (lv_obj_t *panel = make_overlay(radio_setup_)) {
        label(panel, "Radio settings", 16, 9, 260, 22, &lv_font_montserrat_18, kAccent);
        constexpr const char *captions[] = {"Frequency (MHz)", "TX power (dBm)"};
        constexpr const char *arrows[]   = {LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT};
        for (std::size_t i = 0; i < radio_rows_.size(); ++i) {
            auto *row      = make_panel(panel, 16, 36 + i * 28, 260, 24, lv_color_hex(0x242424), LV_OPA_COVER, 4);
            radio_rows_[i] = row;
            if (!row) continue;
            label(row, captions[i], 7, 4, 130, 17, &lv_font_montserrat_12, kText);
            radio_values_[i] =
                make_label(row, "", 162, 4, 72, 17, &lv_font_montserrat_12, lv_color_hex(kText), LV_TEXT_ALIGN_CENTER);
            for (std::size_t direction = 0; direction < radio_arrows_[i].size(); ++direction) {
                auto *button = make_action_button(row, direction == 0 ? 138 : 234, 2, 24, arrows[direction],
                                                  lv_color_hex(0x363636), lv_color_hex(kText), radio_dialog_click_cb);
                radio_arrows_[i][direction] = button;
                if (button) {
                    lv_obj_set_height(button, 20);
                    if (auto *icon = lv_obj_get_child(button, 0)) lv_obj_center(icon);
                }
            }
        }
        radio_setup_status_ = label(panel, "", 16, 93, 260, 16, &lv_font_montserrat_12, kSecondary);
        radio_setup_close_  = make_action_button(panel, 16, 112, 114, "Esc: Skip", lv_color_hex(0x292929),
                                                 lv_color_hex(kText), radio_dialog_click_cb);
        radio_setup_save_   = primary_button(panel, "Enter: Save");
    }
}

void LoraScreen::show_radio_setup(bool initial)
{
    if (!radio_notice_acknowledged_) return;
    radio_draft_  = radio_preferences_.settings;
    radio_field_  = radio_draft_.valid_frequency() && !radio_draft_.valid_power() ? kPower : kFrequency;
    radio_dialog_ = RadioDialog::Setup;
    lv_label_set_text(lv_obj_get_child(radio_setup_close_, 0), initial ? "Esc: Skip" : "Esc: Cancel");
    update_radio_fields();
    set_visible(radio_setup_, true);
    lv_obj_move_foreground(radio_setup_);
}

void LoraScreen::update_radio_fields(const char *status)
{
    const bool configured[] = {radio_draft_.valid_frequency(), radio_draft_.valid_power()};
    char frequency[16]      = "Unset";
    if (configured[kFrequency]) {
        std::snprintf(frequency, sizeof(frequency), "%d.%03d", radio_draft_.frequency_khz / 1000,
                      radio_draft_.frequency_khz % 1000);
    }
    char power[16] = "Unset";
    if (configured[kPower]) std::snprintf(power, sizeof(power), "%d", radio_draft_.tx_power_dbm);
    const char *values[] = {frequency, power};
    for (std::size_t i = 0; i < radio_rows_.size(); ++i) {
        lv_obj_set_style_outline_width(radio_rows_[i], i == radio_field_ ? 1 : 0, LV_PART_MAIN);
        lv_obj_set_style_outline_color(radio_rows_[i], lv_color_hex(kAccent), LV_PART_MAIN);
        lv_obj_set_style_text_color(radio_values_[i], lv_color_hex(configured[i] ? kText : kSecondary), LV_PART_MAIN);
        lv_label_set_text(radio_values_[i], values[i]);
    }
    lv_obj_set_style_text_color(radio_setup_status_, lv_color_hex(status ? kError : kSecondary), LV_PART_MAIN);
    lv_label_set_text(radio_setup_status_, status ? status : "F / X: Switch field   Z / C: Change");
}

void LoraScreen::step_radio_field(std::size_t field, bool forward)
{
    radio_field_ = field;
    if (field == kFrequency) {
        radio_draft_.frequency_khz =
            cap_lora::step_radio_preset(cap_lora::kRadioFrequenciesKhz, radio_draft_.frequency_khz, forward);
    } else {
        radio_draft_.tx_power_dbm =
            cap_lora::step_radio_preset(cap_lora::kRadioPowersDbm, radio_draft_.tx_power_dbm, forward);
    }
    update_radio_fields();
}

bool LoraScreen::handle_radio_dialog_key(uint32_t key)
{
    if (radio_dialog_ == RadioDialog::Notice) {
        if (key == '7' || key == ' ') {
            if (lv_obj_has_state(radio_notice_skip_, LV_STATE_CHECKED))
                lv_obj_remove_state(radio_notice_skip_, LV_STATE_CHECKED);
            else
                lv_obj_add_state(radio_notice_skip_, LV_STATE_CHECKED);
        } else if (key == LV_KEY_ENTER || key == '8') {
            accept_radio_notice();
        }
        return key != LV_KEY_ESC;
    }

    if (key == LV_KEY_ESC) {
        close_radio_setup();
        return true;
    }
    if (key == LV_KEY_ENTER) {
        save_radio_settings();
        return true;
    }
    const uint32_t navigation = lora_app_detail::normalize_lora_key(key, LoraView::INFO);
    if (navigation == LV_KEY_UP || navigation == LV_KEY_DOWN || key == LV_KEY_NEXT || key == LV_KEY_PREV) {
        radio_field_ = 1 - radio_field_;
        update_radio_fields();
    } else if (key == LV_KEY_LEFT || key == 'z' || key == 'Z') {
        step_radio_field(radio_field_, false);
    } else if (key == LV_KEY_RIGHT || key == 'c' || key == 'C') {
        step_radio_field(radio_field_, true);
    }
    return true;
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
        radio_field_ = radio_draft_.valid_frequency() ? kPower : kFrequency;
        update_radio_fields("Select both frequency and TX power.");
        return;
    }

    const auto &current = radio_preferences_.settings;
    if (radio_draft_.frequency_khz == current.frequency_khz && radio_draft_.tx_power_dbm == current.tx_power_dbm) {
        close_radio_setup();
        return;
    }
    if (initialization_pending_ || !pending_tx_text_.empty()) {
        update_radio_fields(initialization_pending_ ? "Wait for LoRa to finish initializing."
                                                    : "Wait for your message to finish sending.");
        return;
    }

    auto preferences     = radio_preferences_;
    preferences.settings = radio_draft_;
    std::string error;
    if (!cap_lora::save_radio_preferences(preferences, error)) {
        spdlog::error("LoRa radio preferences: {}", error);
        update_radio_fields("Could not save settings. Try again.");
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
    if (!self || !self->app_active_) return;
    auto *target = lv_event_get_current_target_obj(event);
    try {
        switch (self->radio_dialog_) {
            case RadioDialog::Notice:
                if (target == self->radio_notice_ok_) self->accept_radio_notice();
                break;
            case RadioDialog::Setup:
                if (target == self->radio_setup_save_)
                    self->save_radio_settings();
                else if (target == self->radio_setup_close_)
                    self->close_radio_setup();
                else {
                    for (std::size_t field = 0; field < self->radio_arrows_.size(); ++field) {
                        const auto &arrows = self->radio_arrows_[field];
                        if (target == arrows[0] || target == arrows[1]) {
                            self->step_radio_field(field, target == arrows[1]);
                            break;
                        }
                    }
                }
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
