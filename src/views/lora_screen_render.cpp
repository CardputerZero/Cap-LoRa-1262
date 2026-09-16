/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_screen.hpp"

namespace lora_app_detail {

constexpr lv_coord_t kScreenWidth         = 320;
constexpr lv_coord_t kScreenHeight        = 170;
constexpr lv_coord_t kContentHeight       = 150;
constexpr uint32_t kViewTransitionMs      = 150;
constexpr uint32_t kMessageTitleHoldMs    = 3200;
constexpr uint32_t kMessageTitleHideMs    = 340;
constexpr lv_coord_t kMessageTitleShownY  = -8;
constexpr lv_coord_t kMessageTitleHiddenY = -29;
constexpr lv_coord_t kBubbleTailWidth     = 6;
constexpr lv_coord_t kBubbleTailDrop      = 3;
constexpr lv_coord_t kSendCursorWidth     = 2;
constexpr lv_coord_t kSendCursorHeight    = 17;
constexpr lv_coord_t kSendInputLetterGap = 2;
constexpr uint32_t kSendCursorColor      = 0x153E8A;
constexpr uint32_t kSendCursorBlinkMs     = 500;
constexpr lv_coord_t kSendActionButtonHeight = 26;
constexpr lv_coord_t kSendActionButtonY      = kScreenHeight - kSendActionButtonHeight;
constexpr lv_coord_t kSendInputTop            = 22;
constexpr lv_coord_t kSendInputButtonGap      = 5;
constexpr lv_coord_t kSendInputHeight         = kSendActionButtonY - kSendInputTop - kSendInputButtonGap;
constexpr lv_coord_t kSendInputTextTop        = 8;
constexpr lv_coord_t kSendInputStatusHeight   = 16;
constexpr lv_coord_t kSendInputStatusGap      = 3;
constexpr lv_coord_t kSendInputStatusY = kSendInputHeight - kSendInputStatusHeight - kSendInputStatusGap;
constexpr lv_coord_t kSendInputTextHeight = kSendInputHeight - kSendInputTextTop - kSendInputStatusGap;
constexpr lv_coord_t kInfoDividerInset     = 12;
constexpr lv_coord_t kInfoTopDividerY      = 57;
constexpr lv_coord_t kInfoStatsY           = 40;
constexpr lv_coord_t kInfoTableY           = 59;
constexpr lv_coord_t kInfoTableWidth       = 304;
constexpr lv_coord_t kInfoTableHeight      = 94;
constexpr lv_coord_t kInfoBottomDividerY   = 154;
constexpr lv_coord_t kInfoLabelWidth       = kInfoTableWidth / 3;
constexpr lv_coord_t kInfoValueWidth       = kInfoTableWidth - kInfoLabelWidth;
constexpr lv_coord_t kInfoRowHeight        = 18;
constexpr lv_coord_t kInfoScrollbarWidth   = 4;
constexpr uint32_t kInfoDividerColor       = 0x4E5157;
constexpr uint32_t kClipboardNoticeColor   = 0x5BA7FF;
constexpr char kNicknameRecolorTag[]       = "#6B4423 ";
constexpr char kReplyNicknameRecolorTag[]  = "#8B2E2E ";
constexpr lv_coord_t kNicknameEditorX      = 8;
constexpr lv_coord_t kNicknameEditorY      = 87;
constexpr lv_coord_t kNicknameEditorWidth  = 304;
constexpr lv_coord_t kNicknameEditorHeight = 68;

static const char *safe_text(const char *text, const char *fallback = "")
{
    return text && text[0] ? text : fallback;
}

static const char *short_pi4io_status(const char *status)
{
    status = safe_text(status, "Unavailable");
    if (std::strstr(status, "write OUT failed")) return "OUT write fail";
    if (std::strstr(status, "write POL failed")) return "POL write fail";
    if (std::strstr(status, "write CFG failed")) return "CFG write fail";
    if (std::strstr(status, "snapshot failed")) return "Snapshot fail";
    if (std::strstr(status, "not found")) return "0x43 not found";
    if (std::strstr(status, "open ") && std::strstr(status, "failed")) return "I2C open fail";
    if (std::strstr(status, "init cancelled")) return "Init cancelled";
    if (std::strstr(status, "init ok")) return "Init OK";
    if (std::strstr(status, "state restored")) return "State restored";
    if (std::strstr(status, "restore failed")) return "Restore fail";
    if (std::strlen(status) > 24) return "PI4IO unavailable";
    return status;
}

}  // namespace lora_app_detail

void LoraScreen::set_visible(lv_obj_t *object, bool visible)
{
    if (!object) return;
    if (visible)
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *LoraScreen::make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                                 lv_color_t color, lv_opa_t opacity, lv_coord_t radius)
{
    if (!parent) return nullptr;
    lv_obj_t *panel = lv_obj_create(parent);
    if (!panel) return nullptr;
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t *LoraScreen::make_plain_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                           lv_coord_t height)
{
    return make_panel(parent, x, y, width, height, lv_color_hex(0x000000), LV_OPA_TRANSP, 0);
}

lv_obj_t *LoraScreen::make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                 lv_coord_t height, const lv_font_t *font, lv_color_t color, lv_text_align_t align)
{
    if (!parent) return nullptr;
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

lv_obj_t *LoraScreen::make_divider(lv_obj_t *parent, lv_coord_t y)
{
    return make_panel(parent, 8, y, 304, 1, lv_color_hex(0x25272B), LV_OPA_COVER, 0);
}

void LoraScreen::bubble_tail_draw_cb(lv_event_t *event) noexcept
{
    if (!event) return;
    lv_obj_t *row     = lv_event_get_target_obj(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!row || !layer) return;
    lv_obj_t *bubble = lv_obj_get_child(row, 0);
    if (!bubble) return;

    lv_area_t area;
    lv_obj_get_coords(bubble, &area);
    lv_draw_triangle_dsc_t draw_dsc;
    lv_draw_triangle_dsc_init(&draw_dsc);
    draw_dsc.color             = lv_obj_get_style_bg_color(bubble, LV_PART_MAIN);
    draw_dsc.opa               = lv_obj_get_style_bg_opa(bubble, LV_PART_MAIN);
    lv_opa_t recursive_opacity = lv_obj_get_style_opa_recursive(bubble, LV_PART_MAIN);
    if (recursive_opacity < LV_OPA_MAX) draw_dsc.opa = LV_OPA_MIX2(draw_dsc.opa, recursive_opacity);

    static constexpr lv_coord_t TAIL_RISE     = 9;
    static constexpr lv_coord_t TAIL_SHOULDER = 10;
    bool outgoing                             = lv_obj_has_flag(row, LV_OBJ_FLAG_USER_1);
    lv_coord_t side_x                         = outgoing ? area.x2 : area.x1;
    lv_coord_t shoulder_x                     = outgoing ? area.x2 - TAIL_SHOULDER : area.x1 + TAIL_SHOULDER;
    lv_coord_t tip_x =
        outgoing ? area.x2 + lora_app_detail::kBubbleTailWidth : area.x1 - lora_app_detail::kBubbleTailWidth;
    draw_dsc.p[0] = {static_cast<lv_value_precise_t>(side_x), static_cast<lv_value_precise_t>(area.y2 - TAIL_RISE)};
    draw_dsc.p[1] = {static_cast<lv_value_precise_t>(shoulder_x), static_cast<lv_value_precise_t>(area.y2)};
    draw_dsc.p[2] = {static_cast<lv_value_precise_t>(tip_x),
                     static_cast<lv_value_precise_t>(area.y2 + lora_app_detail::kBubbleTailDrop)};
    lv_draw_triangle(layer, &draw_dsc);
}

void LoraScreen::update_page_indicator()
{
    if (!page_dots_[0] || !page_dots_[1]) return;
    bool messages_active = model_.view() == LoraView::MESSAGES;
    lv_obj_set_style_bg_color(page_dots_[0], lv_color_hex(messages_active ? 0xE4E4E4 : 0x4E5157),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(page_dots_[1], lv_color_hex(messages_active ? 0x4E5157 : 0xE4E4E4),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

void LoraScreen::update_info_content()
{
    if (!info_status_label_ || !info_status_dot_ || !info_stats_label_) return;
    const char *state_text = "RECEIVING";
    uint32_t state_color   = 0x69AD80;
    if (initialization_pending_) {
        state_text  = "INITIALIZING";
        state_color = 0xC9A45C;
    } else if (!lora_info_.hw_ready) {
        state_text  = "RADIO OFF";
        state_color = 0xD96C6C;
    } else if (lora_info_.tx_in_progress || lora_info_.tx_mode) {
        state_text  = lora_info_.tx_in_progress ? "SENDING" : "TX MODE";
        state_color = 0xC9A45C;
    }
    lv_label_set_text(info_status_label_, state_text);
    lv_obj_set_style_bg_color(info_status_dot_, lv_color_hex(state_color), LV_PART_MAIN | LV_STATE_DEFAULT);

    const auto set_row = [this](InfoRowId row, const char *text) {
        lv_obj_t *label = info_value_labels_[static_cast<std::size_t>(row)];
        if (label) lv_label_set_text(label, text ? text : "");
    };
    char value[192] = {};
    set_row(InfoRowId::Nickname, model_.nickname().c_str());
    if (lora_info_.rx_count > 0) {
        std::snprintf(value, sizeof(value), "%.0f dBm", lora_info_.rssi);
        set_row(InfoRowId::Rssi, value);
        std::snprintf(value, sizeof(value), "%.1f dB", lora_info_.snr);
        set_row(InfoRowId::Snr, value);
    } else {
        set_row(InfoRowId::Rssi, "--");
        set_row(InfoRowId::Snr, "--");
    }
    set_row(InfoRowId::Radio, "SX1262");
    std::snprintf(value, sizeof(value), "%.3f MHz", lora_info_.frequency_mhz);
    set_row(InfoRowId::Frequency, value);
    std::snprintf(value, sizeof(value), "v%s", CAP_LORA_APP_VERSION);
    set_row(InfoRowId::Version, value);
    set_row(InfoRowId::Modulation, "LoRa");
    std::snprintf(value, sizeof(value), "%.0f kHz", lora_info_.bandwidth_khz);
    set_row(InfoRowId::Bandwidth, value);
    std::snprintf(value, sizeof(value), "SF%u", static_cast<unsigned>(lora_info_.spreading_factor));
    set_row(InfoRowId::SpreadingFactor, value);
    std::snprintf(value, sizeof(value), "4/%u", static_cast<unsigned>(lora_info_.coding_rate));
    set_row(InfoRowId::CodingRate, value);
    std::snprintf(value, sizeof(value), "%d dBm", static_cast<int>(lora_info_.output_power_dbm));
    set_row(InfoRowId::Power, value);
    std::snprintf(value, sizeof(value), "%u symbols", static_cast<unsigned>(lora_info_.preamble_symbols));
    set_row(InfoRowId::Preamble, value);
    std::snprintf(value, sizeof(value), "0x%02X", static_cast<unsigned>(lora_info_.sync_word));
    set_row(InfoRowId::SyncWord, value);
    std::snprintf(value, sizeof(value), "%.1f V", lora_info_.tcxo_voltage);
    set_row(InfoRowId::TcxoVoltage, value);
    std::snprintf(value, sizeof(value), "%.0f mA", lora_info_.current_limit_ma);
    set_row(InfoRowId::CurrentLimit, value);
    std::snprintf(value, sizeof(value), "%zu B", lora_chat_protocol::kMaxMessageBytes);
    set_row(InfoRowId::PayloadLimit, value);
    if (lora_info_.spi_speed_hz % 1000000U == 0)
        std::snprintf(value, sizeof(value), "%u MHz  %s", static_cast<unsigned>(lora_info_.spi_speed_hz / 1000000U),
                      lora_app_detail::safe_text(lora_info_.spi_device, "Unavailable"));
    else
        std::snprintf(value, sizeof(value), "%u kHz  %s", static_cast<unsigned>(lora_info_.spi_speed_hz / 1000U),
                      lora_app_detail::safe_text(lora_info_.spi_device, "Unavailable"));
    set_row(InfoRowId::SpiDevice, value);
    set_row(InfoRowId::Pi4io, lora_app_detail::short_pi4io_status(lora_info_.pi4io_status));

    if (initialization_pending_ || !lora_info_.hw_ready) {
        lv_label_set_text(info_stats_label_, lora_app_detail::safe_text(lora_info_.diag, "No diagnostics"));
        lv_obj_set_style_text_color(info_stats_label_, lv_color_hex(initialization_pending_ ? 0xC9A45C : 0xD96C6C),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        std::snprintf(value, sizeof(value), "CRC %s   RX:%llu  TX:%llu", lora_info_.rx_count > 0 ? "OK" : "--",
                      static_cast<unsigned long long>(lora_info_.rx_count),
                      static_cast<unsigned long long>(lora_info_.tx_count));
        lv_label_set_text(info_stats_label_, value);
        lv_obj_set_style_text_color(info_stats_label_, lv_color_hex(lora_info_.rx_count > 0 ? 0x69AD80 : 0x777B82),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void LoraScreen::configure_editor_layout()
{
    if (!send_view_ || !send_title_label_ || !send_input_bubble_ || !send_input_label_ || !send_cursor_label_ ||
        !send_status_label_ || !send_cancel_button_ || !send_confirm_button_)
        return;
    const bool nickname = model_.editor_mode() == LoraEditorMode::NICKNAME;
    const auto configure_button = [](lv_obj_t *button, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                                     const char *text, const lv_font_t *font) {
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, height);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        if (!label) return;
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(label);
    };

    if (nickname) {
        lv_obj_set_pos(send_view_, lora_app_detail::kNicknameEditorX, lora_app_detail::kNicknameEditorY);
        lv_obj_set_size(send_view_, lora_app_detail::kNicknameEditorWidth, lora_app_detail::kNicknameEditorHeight);
        lv_obj_set_style_bg_color(send_view_, lv_color_hex(0x474747), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(send_view_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(send_view_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(send_title_label_, "New Nickname");
        lv_obj_set_pos(send_title_label_, 8, 3);
        lv_obj_set_size(send_title_label_, 150, 12);
        lv_obj_set_style_text_font(send_title_label_, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(send_title_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(send_input_bubble_, 8, 17);
        lv_obj_set_size(send_input_bubble_, 288, 28);
        lv_obj_set_style_radius(send_input_bubble_, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(send_input_label_, 6, 6);
        lv_obj_set_size(send_input_label_, 276, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(send_input_label_, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_pos(send_status_label_, 92, 52);
        lv_obj_set_size(send_status_label_, 108, 11);
        lv_obj_set_style_text_font(send_status_label_, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(send_status_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        configure_button(send_cancel_button_, 8, 48, 80, 17, "ESC: Cancel", &lv_font_montserrat_10);
        configure_button(send_confirm_button_, 204, 48, 92, 17, "Enter: Save", &lv_font_montserrat_10);
        return;
    }

    lv_obj_set_pos(send_view_, 0, 0);
    lv_obj_set_size(send_view_, lora_app_detail::kScreenWidth, lora_app_detail::kScreenHeight);
    lv_obj_set_style_bg_opa(send_view_, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(send_view_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(send_title_label_, model_.reply_to().empty() ? "New Message" : "Reply Message");
    lv_obj_set_pos(send_title_label_, 0, 0);
    lv_obj_set_size(send_title_label_, 320, 18);
    lv_obj_set_style_text_font(send_title_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(send_title_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(send_input_bubble_, 17, lora_app_detail::kSendInputTop);
    lv_obj_set_size(send_input_bubble_, 286, lora_app_detail::kSendInputHeight);
    lv_obj_set_style_radius(send_input_bubble_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(send_input_label_, 10, lora_app_detail::kSendInputTextTop);
    lv_obj_set_size(send_input_label_, 266, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(send_input_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_pos(send_status_label_, 17 + 10,
                   lora_app_detail::kSendInputTop + lora_app_detail::kSendInputStatusY);
    lv_obj_set_size(send_status_label_, 266, lora_app_detail::kSendInputStatusHeight);
    lv_obj_set_style_text_font(send_status_label_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(send_status_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    configure_button(send_cancel_button_, 17, lora_app_detail::kSendActionButtonY, 110,
                     lora_app_detail::kSendActionButtonHeight, "ESC: Cancel", &lv_font_montserrat_14);
    configure_button(send_confirm_button_, 203, lora_app_detail::kSendActionButtonY, 100,
                     lora_app_detail::kSendActionButtonHeight, "Enter: Send", &lv_font_montserrat_14);
}

void LoraScreen::update_send_content()
{
    if (!send_input_label_ || !send_status_label_) return;
    configure_editor_layout();
    lv_label_set_text(send_input_label_, model_.tx_input().c_str());
    lv_obj_set_style_text_color(send_input_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    const bool pasted = clipboard_notice_ == ClipboardNotice::Pasted ||
                        clipboard_notice_ == ClipboardNotice::PastedTruncated;
    const bool has_status = pasted || !model_.send_status().empty();
    lv_label_set_text(send_status_label_, clipboard_notice_ == ClipboardNotice::PastedTruncated
                                              ? "pasted (truncated)"
                                              : pasted ? "pasted" : model_.send_status().c_str());
    set_visible(send_status_label_, has_status);
    update_send_cursor();
}

void LoraScreen::update_send_cursor()
{
    if (!send_cursor_label_ || !send_input_label_) return;
    lv_obj_update_layout(send_input_label_);
    lv_point_t position{};
    lv_label_get_letter_pos(send_input_label_, model_.cursor_position(), &position);
    const bool nickname = model_.editor_mode() == LoraEditorMode::NICKNAME;
    const lv_coord_t text_top = nickname ? 6 : lora_app_detail::kSendInputTextTop;
    const lv_coord_t cursor_height = nickname ? 14 : lora_app_detail::kSendCursorHeight;
    lv_obj_set_size(send_cursor_label_, lora_app_detail::kSendCursorWidth, cursor_height);
    const bool pasted = clipboard_notice_ == ClipboardNotice::Pasted ||
                        clipboard_notice_ == ClipboardNotice::PastedTruncated;
    const lv_coord_t viewport_height = !pasted && model_.send_status().empty()
                                           ? lora_app_detail::kSendInputTextHeight
                                           : lora_app_detail::kSendInputStatusY -
                                                 lora_app_detail::kSendInputTextTop -
                                                 lora_app_detail::kSendInputStatusGap;
    const lv_coord_t scroll_offset = nickname ? 0 : std::max<lv_coord_t>(0, position.y + cursor_height - viewport_height);
    lv_obj_set_y(send_input_label_, text_top - scroll_offset);
    const lv_coord_t cursor_gap = position.x > 0
                                      ? (lora_app_detail::kSendInputLetterGap + lora_app_detail::kSendCursorWidth) / 2
                                      : 0;
    lv_obj_set_pos(send_cursor_label_, lv_obj_get_x(send_input_label_) + position.x - cursor_gap,
                   lv_obj_get_y(send_input_label_) + position.y);
    const bool visible = model_.editor_mode() != LoraEditorMode::NONE &&
                         (lv_tick_get() / lora_app_detail::kSendCursorBlinkMs) % 2 == 0;
    set_visible(send_cursor_label_, visible);
}

void LoraScreen::move_send_cursor_vertical(int direction)
{
    if (!send_input_label_ || direction == 0) return;
    lv_point_t position{};
    lv_label_get_letter_pos(send_input_label_, model_.cursor_position(), &position);
    const lv_coord_t line_height = lv_font_get_line_height(lv_obj_get_style_text_font(send_input_label_, LV_PART_MAIN));
    const lv_coord_t line_step = line_height + lv_obj_get_style_text_line_space(send_input_label_, LV_PART_MAIN);
    position.y += direction * line_step + line_height / 2;
    const uint32_t target = lv_label_get_letter_on(send_input_label_, &position, false);
    if (target != model_.cursor_position()) model_.set_cursor_position(target);
}

void LoraScreen::scroll_to_latest(lv_anim_enable_t animation)
{
    if (!message_list_ || !last_message_row_) {
        scroll_to_latest_pending_ = false;
        return;
    }
    lv_obj_update_layout(message_list_);
    lv_obj_scroll_to_view(last_message_row_, animation);
    scroll_to_latest_pending_ = false;
}

void LoraScreen::schedule_message_title_dismissal()
{
    if (!messages_title_ || message_title_timer_ || lv_obj_has_flag(messages_title_, LV_OBJ_FLAG_HIDDEN)) return;

    message_title_timer_ =
        lv_timer_create(&LoraScreen::static_message_title_timer_cb, lora_app_detail::kMessageTitleHoldMs, this);
    if (message_title_timer_) lv_timer_set_repeat_count(message_title_timer_, 1);
}

void LoraScreen::cancel_message_title_animation()
{
    if (message_title_timer_) {
        lv_timer_delete(message_title_timer_);
        message_title_timer_ = nullptr;
    }
    if (messages_title_) lv_anim_del(messages_title_, &LoraScreen::message_title_anim_exec_cb);
}

void LoraScreen::show_message_notice(const char *text)
{
    if (!messages_title_) return;
    cancel_message_title_animation();
    lv_point_t text_size{};
    lv_text_get_size(&text_size, text ? text : "", &lv_font_montserrat_18, 0, 0,
                     lora_app_detail::kScreenWidth, LV_TEXT_FLAG_NONE);
    lv_obj_t *label = lv_obj_get_child(messages_title_, 0);
    if (label) {
        lv_label_set_text(label, text ? text : "");
        lv_obj_set_pos(label, 0, 0);
        lv_obj_set_size(label, text_size.x, text_size.y);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(lora_app_detail::kClipboardNoticeColor),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_pos(messages_title_, (lora_app_detail::kScreenWidth - text_size.x) / 2,
                   lora_app_detail::kContentHeight - text_size.y - 4);
    lv_obj_set_size(messages_title_, text_size.x, text_size.y);
    lv_obj_set_style_bg_opa(messages_title_, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(messages_title_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_visible(messages_title_, true);
    lv_obj_move_foreground(messages_title_);
}

void LoraScreen::hide_message_notice()
{
    if (!messages_title_) return;
    lv_anim_del(messages_title_, &LoraScreen::message_title_anim_exec_cb);
    set_visible(messages_title_, false);
}

void LoraScreen::dismiss_message_title()
{
    if (clipboard_notice_ == ClipboardNotice::Copied || clipboard_notice_ == ClipboardNotice::Wait) return;
    if (!messages_title_ || lv_obj_has_flag(messages_title_, LV_OBJ_FLAG_HIDDEN)) return;

    if (message_title_timer_) {
        lv_timer_delete(message_title_timer_);
        message_title_timer_ = nullptr;
    }

    lv_anim_del(messages_title_, &LoraScreen::message_title_anim_exec_cb);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, messages_title_);
    lv_anim_set_values(&animation, lv_obj_get_y(messages_title_), lora_app_detail::kMessageTitleHiddenY);
    lv_anim_set_time(&animation, lora_app_detail::kMessageTitleHideMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, &LoraScreen::message_title_anim_exec_cb);
    lv_anim_set_user_data(&animation, messages_title_);
    lv_anim_set_completed_cb(&animation, &LoraScreen::hide_message_title_after_anim_cb);
    if (!lv_anim_start(&animation)) {
        message_title_anim_exec_cb(messages_title_, lora_app_detail::kMessageTitleHiddenY);
        set_visible(messages_title_, false);
    }
}

void LoraScreen::message_title_anim_exec_cb(void *object, int32_t y) noexcept
{
    if (!object) return;
    lv_obj_set_y(static_cast<lv_obj_t *>(object), static_cast<lv_coord_t>(y));
}

void LoraScreen::hide_message_title_after_anim_cb(lv_anim_t *animation) noexcept
{
    if (!animation) return;
    auto *title = static_cast<lv_obj_t *>(lv_anim_get_user_data(animation));
    if (!lora_animation_callback_allowed(title)) return;
    message_title_anim_exec_cb(title, lora_app_detail::kMessageTitleHiddenY);
    set_visible(title, false);
}

void LoraScreen::view_opa_exec_cb(void *object, int32_t opacity) noexcept
{
    if (!lora_animation_callback_allowed(object)) return;
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), static_cast<lv_opa_t>(opacity),
                         LV_PART_MAIN | LV_STATE_DEFAULT);
}

void LoraScreen::hide_view_after_fade_cb(lv_anim_t *animation) noexcept
{
    if (!animation) return;
    auto *view = static_cast<lv_obj_t *>(lv_anim_get_user_data(animation));
    if (!lora_animation_callback_allowed(view)) return;
    set_visible(view, false);
    lv_obj_set_style_opa(view, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void LoraScreen::cancel_view_animation(lv_obj_t *view)
{
    if (view) lv_anim_del(view, view_opa_exec_cb);
}

void LoraScreen::cancel_view_animations()
{
    cancel_view_animation(messages_view_);
    cancel_view_animation(info_view_);
    cancel_view_animation(send_view_);
}

void LoraScreen::animate_view_opacity(lv_obj_t *view, lv_opa_t start, lv_opa_t end, bool hide_after_fade)
{
    if (!view) return;
    cancel_view_animation(view);
    set_visible(view, true);
    view_opa_exec_cb(view, start);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view);
    lv_anim_set_values(&animation, start, end);
    lv_anim_set_time(&animation, lora_app_detail::kViewTransitionMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&animation, view_opa_exec_cb);
    if (hide_after_fade) {
        lv_anim_set_user_data(&animation, view);
        lv_anim_set_completed_cb(&animation, hide_view_after_fade_cb);
    }
    if (!lv_anim_start(&animation)) {
        view_opa_exec_cb(view, end);
        if (hide_after_fade) {
            set_visible(view, false);
            view_opa_exec_cb(view, LV_OPA_COVER);
        }
    }
}

void LoraScreen::transition_to_view(lv_obj_t *target)
{
    if (!target || target == active_view_) return;
    if (!active_view_) {
        lv_obj_t *views[] = {messages_view_, info_view_, send_view_};
        for (lv_obj_t *view : views) {
            cancel_view_animation(view);
            lv_obj_set_style_opa(view, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
            set_visible(view, view == target);
        }
        active_view_ = target;
        return;
    }

    lv_obj_t *outgoing      = active_view_;
    lv_opa_t outgoing_start = lv_obj_get_style_opa(outgoing, LV_PART_MAIN);
    lv_opa_t incoming_start = lv_obj_has_flag(target, LV_OBJ_FLAG_HIDDEN)
                                  ? static_cast<lv_opa_t>(LV_OPA_TRANSP)
                                  : lv_obj_get_style_opa(target, LV_PART_MAIN);
    animate_view_opacity(outgoing, outgoing_start, LV_OPA_TRANSP, true);
    animate_view_opacity(target, incoming_start, LV_OPA_COVER, false);
    active_view_ = target;
}

void LoraScreen::render_current_view()
{
    bool show_messages = model_.view() == LoraView::MESSAGES;
    bool show_info     = model_.view() == LoraView::INFO;
    bool show_send     = model_.editor_mode() == LoraEditorMode::MESSAGE;
    bool show_nickname = model_.editor_mode() == LoraEditorMode::NICKNAME;
    const bool send_is_overlay =
        send_view_ && active_view_ != send_view_ && !lv_obj_has_flag(send_view_, LV_OBJ_FLAG_HIDDEN);
    if (show_info) update_info_content();
    if (show_send || show_nickname) update_send_content();
    transition_to_view(show_send ? send_view_ : (show_messages ? messages_view_ : info_view_));
    if (show_nickname) {
        set_visible(send_view_, true);
        lv_obj_move_foreground(send_view_);
    } else if (!show_send && send_is_overlay) {
        set_visible(send_view_, false);
    }
    set_visible(page_indicator_, !show_send && !show_nickname);
    if (!show_send && !show_nickname) update_page_indicator();
    if (show_messages && scroll_to_latest_pending_ && !model_.selected_message_index()) scroll_to_latest(LV_ANIM_OFF);
    if (help_view_ && !lv_obj_has_flag(help_view_, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(help_view_);
}

void LoraScreen::create_ui()
{
    page_root_ = make_panel(root_screen_, 0, 0, lora_app_detail::kScreenWidth, lora_app_detail::kScreenHeight,
                            lv_color_hex(0x0B0C0E), LV_OPA_COVER, 0);
    if (!page_root_) return;
    lv_obj_add_event_cb(page_root_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    create_messages_view();
    create_info_view();
    create_send_view();
    create_page_indicator();
    create_help_view();
    lv_obj_t *persistent_children[] = {
        empty_message_label_,     empty_message_hint_label_, info_status_dot_,         info_status_label_,
        info_change_name_button_, info_stats_label_,         info_table_,              info_table_content_,
        send_title_label_,        send_input_bubble_,        send_input_label_,        send_cursor_label_,
        send_status_label_,       send_cancel_button_,       send_confirm_button_,     page_dots_[0],
        page_dots_[1],            messages_title_};
    for (lv_obj_t *object : persistent_children) track_owned_handle(object);
    for (lv_obj_t *object : info_value_labels_) track_owned_handle(object);
}

void LoraScreen::create_help_view()
{
    help_view_ = make_panel(page_root_, 0, 0, lora_app_detail::kScreenWidth, lora_app_detail::kScreenHeight,
                            lv_color_hex(0x000000), LV_OPA_COVER, 0);
    if (!help_view_) return;
    lv_obj_add_event_cb(help_view_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    make_label(help_view_, "esc", 8, 4, 32, 14, &lv_font_montserrat_10, lv_color_hex(0xF2C94C),
               LV_TEXT_ALIGN_LEFT);
    make_label(help_view_,
               "Connect Cap LoRa-1262 to send and receive messages over LoRa, with support for group communication "
               "between multiple devices.\n\nFeatures: nickname, message selection and replies, copy and paste, plus radio "
               "and link details in Info.\n\nKeyboard: compose a message\nF / X / Z / C: switch between screens\n"
               "Fn + F / X: select a message\nEnter: reply to selection\nCtrl + C / V: copy / paste",
               8, 20, 304, 146, &lv_font_montserrat_10, lv_color_hex(0xE4E4E4), LV_TEXT_ALIGN_LEFT);
    set_visible(help_view_, false);
}

void LoraScreen::create_messages_view()
{
    messages_view_ = make_plain_container(page_root_, 0, 0, 320, 150);
    if (!messages_view_) return;
    lv_obj_add_event_cb(messages_view_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    message_list_ = make_plain_container(messages_view_, 0, 0, 320, 150);
    if (!message_list_) return;
    lv_obj_add_event_cb(message_list_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_set_flex_flow(message_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(message_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(message_list_, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(message_list_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(message_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(message_list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(message_list_, LV_SCROLLBAR_MODE_OFF);

    empty_message_label_      = make_label(messages_view_, "No messages yet", 0, 50, 320, 16, &lv_font_montserrat_12,
                                           lv_color_hex(0xB2B2B2), LV_TEXT_ALIGN_CENTER);
    empty_message_hint_label_ = make_label(messages_view_, "Type anything to send", 0, 68, 320, 16,
                                           &lv_font_montserrat_12, lv_color_hex(0x5FE492), LV_TEXT_ALIGN_CENTER);
    messages_title_           = make_panel(messages_view_, 108, lora_app_detail::kMessageTitleShownY, 104, 28,
                                           lv_color_hex(0x0B0C0E), LV_OPA_COVER, 8);
    if (messages_title_)
        make_label(messages_title_, "Messages", 0, 8, 104, 18, &lv_font_montserrat_14, lv_color_hex(0xE4E4E4),
                   LV_TEXT_ALIGN_CENTER);
}

void LoraScreen::create_info_view()
{
    info_view_ = make_plain_container(page_root_, 0, 0, lora_app_detail::kScreenWidth, lora_app_detail::kScreenHeight);
    if (!info_view_) return;
    lv_obj_add_event_cb(info_view_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    make_label(info_view_, "LoRa Info", 0, 0, 320, 18, &lv_font_montserrat_14, lv_color_hex(0xE4E4E4),
               LV_TEXT_ALIGN_CENTER);
    info_status_dot_ = make_panel(info_view_, 8, 27, 6, 6, lv_color_hex(0x69AD80), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    info_status_label_ =
        make_label(info_view_, "", 20, 22, 160, 18, &lv_font_montserrat_14, lv_color_hex(0xDDE0E4), LV_TEXT_ALIGN_LEFT);
    info_change_name_button_ = make_action_button(info_view_, 188, 20, 132, "Enter: Nickname", lv_color_hex(0xFED40D),
                                                   lv_color_hex(0x5E4D00), &LoraScreen::static_nickname_button_cb);
    if (info_change_name_button_) {
        lv_obj_set_size(info_change_name_button_, 132, 20);
        lv_obj_t *label = lv_obj_get_child(info_change_name_button_, 0);
        if (label) lv_obj_center(label);
    }
    make_panel(info_view_, lora_app_detail::kInfoDividerInset, lora_app_detail::kInfoTopDividerY,
               lora_app_detail::kScreenWidth - lora_app_detail::kInfoDividerInset * 2, 1, lv_color_hex(0x25272B),
               LV_OPA_COVER, 0);
    make_panel(info_view_, lora_app_detail::kInfoDividerInset, lora_app_detail::kInfoBottomDividerY,
               lora_app_detail::kScreenWidth - lora_app_detail::kInfoDividerInset * 2, 1, lv_color_hex(0x25272B),
               LV_OPA_COVER, 0);
    info_stats_label_ = make_label(info_view_, "", 8, lora_app_detail::kInfoStatsY,
                                   lora_app_detail::kInfoTableWidth, lora_app_detail::kInfoRowHeight,
                                   &lv_font_montserrat_14, lv_color_hex(0x777B82), LV_TEXT_ALIGN_LEFT);
    if (info_stats_label_) lv_label_set_long_mode(info_stats_label_, LV_LABEL_LONG_DOT);

    info_table_ = make_plain_container(info_view_, 0, lora_app_detail::kInfoTableY, lora_app_detail::kScreenWidth,
                                       lora_app_detail::kInfoTableHeight);
    if (!info_table_) return;
    lv_obj_add_flag(info_table_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(info_table_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(info_table_, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(info_table_, lora_app_detail::kInfoScrollbarWidth, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(info_table_, lv_color_hex(0x4E5157), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(info_table_, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(info_table_, lora_app_detail::kInfoScrollbarWidth / 2, LV_PART_SCROLLBAR);
    info_table_content_ = make_plain_container(
        info_table_, 0, 0, lora_app_detail::kInfoTableWidth,
        lora_app_detail::kInfoRowHeight * static_cast<lv_coord_t>(INFO_ROW_COUNT));
    if (!info_table_content_) return;

    constexpr std::array<const char *, INFO_ROW_COUNT> captions{
        "Nickname",       "Signal (RSSI)", "SNR (dB)",        "Radio Chip",    "Frequency",    "Version",
        "Modulation",     "Bandwidth",     "Spread Factor",   "Coding Rate",   "TX Power",     "Preamble",
        "Sync Word",      "TCXO Voltage",  "Current Limit",   "Payload Limit", "SPI Device",   "PI4IO",
    };
    for (std::size_t index = 0; index < INFO_ROW_COUNT; ++index) {
        const lv_coord_t y = static_cast<lv_coord_t>(index) * lora_app_detail::kInfoRowHeight;
        lv_obj_t *caption = make_label(info_table_content_, captions[index], 0, y, lora_app_detail::kInfoLabelWidth,
                                       lora_app_detail::kInfoRowHeight, &lv_font_montserrat_14,
                                       lv_color_hex(0x777B82), LV_TEXT_ALIGN_CENTER);
        if (caption) lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
        info_value_labels_[index] = make_label(info_table_content_, "", lora_app_detail::kInfoLabelWidth, y,
                                               lora_app_detail::kInfoValueWidth, lora_app_detail::kInfoRowHeight,
                                               &lv_font_montserrat_14, lv_color_hex(0xDDE0E4), LV_TEXT_ALIGN_CENTER);
        if (info_value_labels_[index]) lv_label_set_long_mode(info_value_labels_[index], LV_LABEL_LONG_DOT);
        if (index > 0)
            make_panel(info_table_content_, 0, y, lora_app_detail::kInfoTableWidth, 1,
                       lv_color_hex(lora_app_detail::kInfoDividerColor), LV_OPA_COVER, 0);
    }
    make_panel(info_view_, lora_app_detail::kInfoLabelWidth, lora_app_detail::kInfoTableY, 1,
               lora_app_detail::kInfoTableHeight, lv_color_hex(lora_app_detail::kInfoDividerColor), LV_OPA_COVER, 0);
}

void LoraScreen::create_send_view()
{
    send_view_ = make_plain_container(page_root_, 0, 0, lora_app_detail::kScreenWidth,
                                      lora_app_detail::kScreenHeight);
    if (!send_view_) return;
    lv_obj_add_event_cb(send_view_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    send_title_label_ = make_label(send_view_, "New Message", 0, 0, 320, 18, &lv_font_montserrat_14,
                                   lv_color_hex(0xE4E4E4), LV_TEXT_ALIGN_CENTER);
    send_input_bubble_ = make_panel(send_view_, 17, lora_app_detail::kSendInputTop, 286,
                                    lora_app_detail::kSendInputHeight, lv_color_hex(0x555555), LV_OPA_COVER, 8);
    send_input_label_  = make_label(send_input_bubble_, "", 10, lora_app_detail::kSendInputTextTop, 266,
                                    LV_SIZE_CONTENT, &lv_font_montserrat_14,
                                    lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);
    if (send_input_label_)
        lv_obj_set_style_text_letter_space(send_input_label_, lora_app_detail::kSendInputLetterGap,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
    send_cursor_label_ = make_panel(send_input_bubble_, 10, lora_app_detail::kSendInputTextTop,
                                    lora_app_detail::kSendCursorWidth,
                                    lora_app_detail::kSendCursorHeight,
                                    lv_color_hex(lora_app_detail::kSendCursorColor), LV_OPA_COVER, 0);
    send_status_label_ = make_label(send_view_, "", 17 + 10,
                                    lora_app_detail::kSendInputTop + lora_app_detail::kSendInputStatusY,
                                    266, lora_app_detail::kSendInputStatusHeight, &lv_font_montserrat_14,
                                    lv_color_hex(0xFED40D), LV_TEXT_ALIGN_RIGHT);
    set_visible(send_status_label_, false);
    send_cancel_button_  = make_action_button(send_view_, 17, lora_app_detail::kSendActionButtonY, 110, "ESC: Cancel",
                                              lv_color_hex(0x6D6D6D),
                                              lv_color_hex(0xF3F3F3), &LoraScreen::static_cancel_button_cb);
    send_confirm_button_ = make_action_button(send_view_, 203, lora_app_detail::kSendActionButtonY, 100, "Enter: Send",
                                              lv_color_hex(0xFED40D),
                                              lv_color_hex(0x5E4D00), &LoraScreen::static_send_button_cb);
    configure_editor_layout();
}

lv_obj_t *LoraScreen::make_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                         const char *text, lv_color_t background, lv_color_t foreground,
                                         lv_event_cb_t callback)
{
    lv_obj_t *button = make_panel(parent, x, y, width, lora_app_detail::kSendActionButtonHeight, background,
                                  LV_OPA_COVER, 5);
    if (!button) return nullptr;
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);
    lv_obj_t *label = make_label(button, text, 0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT, &lv_font_montserrat_14,
                                 foreground, LV_TEXT_ALIGN_CENTER);
    if (label) lv_obj_center(label);
    return button;
}

void LoraScreen::create_page_indicator()
{
    page_indicator_ = make_panel(page_root_, 141, 157, 38, 24, lv_color_hex(0x0B0C0E), LV_OPA_COVER, 7);
    if (!page_indicator_) return;
    lv_obj_add_event_cb(page_indicator_, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    page_dots_[0] = make_panel(page_indicator_, 11, 3, 5, 5, lv_color_hex(0xE4E4E4), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    page_dots_[1] = make_panel(page_indicator_, 22, 3, 5, 5, lv_color_hex(0x4E5157), LV_OPA_COVER, LV_RADIUS_CIRCLE);
}

void LoraScreen::track_owned_handle(lv_obj_t *object)
{
    if (object) lv_obj_add_event_cb(object, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
}

bool LoraScreen::ui_ready() const
{
    if (!lora_page_controls_ready(page_root_, messages_view_, message_list_, empty_message_label_,
                                  empty_message_hint_label_, info_view_, info_status_dot_, info_status_label_,
                                  info_change_name_button_, info_stats_label_, info_table_, info_table_content_,
                                  send_view_, send_title_label_, send_input_bubble_, send_input_label_, send_cursor_label_,
                                  send_status_label_, send_cancel_button_, send_confirm_button_, page_indicator_,
                                  page_dots_[0], page_dots_[1]))
        return false;
    return std::all_of(info_value_labels_.begin(), info_value_labels_.end(), [](lv_obj_t *label) { return label; });
}

void LoraScreen::detach_delete_callbacks()
{
    lv_obj_t *objects[] = {message_list_,
                           help_view_,
                           messages_view_,
                           empty_message_label_,
                           empty_message_hint_label_,
                           messages_title_,
                           info_view_,
                           info_status_dot_,
                           info_status_label_,
                           info_change_name_button_,
                           info_stats_label_,
                           info_table_,
                           info_table_content_,
                           send_view_,
                           send_title_label_,
                           send_input_bubble_,
                           send_input_label_,
                           send_cursor_label_,
                           send_status_label_,
                           send_cancel_button_,
                           send_confirm_button_,
                           page_indicator_,
                           page_dots_[0],
                           page_dots_[1],
                           page_root_};
    for (lv_obj_t *object : objects)
        if (object) lv_obj_remove_event_cb_with_user_data(object, static_owned_obj_delete_cb, this);
    for (lv_obj_t *object : info_value_labels_)
        if (object) lv_obj_remove_event_cb_with_user_data(object, static_owned_obj_delete_cb, this);
}

void LoraScreen::clear_deleted_handles(lv_obj_t *deleted)
{
    if (!deleted) return;
    if (deleted == help_view_) help_view_ = nullptr;
    if (deleted == messages_title_) {
        lv_anim_del(deleted, &LoraScreen::message_title_anim_exec_cb);
        messages_title_ = nullptr;
    }
    if (deleted == empty_message_label_) empty_message_label_ = nullptr;
    if (deleted == empty_message_hint_label_) empty_message_hint_label_ = nullptr;
    if (deleted == info_status_dot_) info_status_dot_ = nullptr;
    if (deleted == info_status_label_) info_status_label_ = nullptr;
    if (deleted == info_change_name_button_) info_change_name_button_ = nullptr;
    if (deleted == info_stats_label_) info_stats_label_ = nullptr;
    if (deleted == info_table_) info_table_ = nullptr;
    if (deleted == info_table_content_) info_table_content_ = nullptr;
    for (lv_obj_t *&label : info_value_labels_)
        if (deleted == label) label = nullptr;
    if (deleted == send_title_label_) send_title_label_ = nullptr;
    if (deleted == send_input_bubble_) send_input_bubble_ = nullptr;
    if (deleted == send_input_label_) send_input_label_ = nullptr;
    if (deleted == send_cursor_label_) send_cursor_label_ = nullptr;
    if (deleted == send_status_label_) send_status_label_ = nullptr;
    if (deleted == send_cancel_button_) send_cancel_button_ = nullptr;
    if (deleted == send_confirm_button_) send_confirm_button_ = nullptr;
    if (deleted == page_dots_[0]) page_dots_[0] = nullptr;
    if (deleted == page_dots_[1]) page_dots_[1] = nullptr;
    if (deleted == message_list_) {
        message_list_     = nullptr;
        last_message_row_ = nullptr;
    }
    if (deleted == messages_view_) {
        if (clipboard_notice_timer_) { lv_timer_delete(clipboard_notice_timer_); clipboard_notice_timer_ = nullptr; }
        clipboard_notice_ = ClipboardNotice::None;
        cancel_message_title_animation();
        messages_view_            = nullptr;
        message_list_             = nullptr;
        messages_title_           = nullptr;
        empty_message_label_      = nullptr;
        empty_message_hint_label_ = nullptr;
        last_message_row_         = nullptr;
    }
    if (deleted == info_view_) {
        info_view_         = nullptr;
        info_status_dot_   = nullptr;
        info_status_label_ = nullptr;
        info_change_name_button_ = nullptr;
        info_stats_label_        = nullptr;
        info_table_              = nullptr;
        info_table_content_      = nullptr;
        info_value_labels_.fill(nullptr);
    }
    if (deleted == send_view_) {
        if (clipboard_notice_ == ClipboardNotice::Pasted ||
            clipboard_notice_ == ClipboardNotice::PastedTruncated) {
            if (clipboard_notice_timer_) { lv_timer_delete(clipboard_notice_timer_); clipboard_notice_timer_ = nullptr; }
            clipboard_notice_ = ClipboardNotice::None;
        }
        send_view_           = nullptr;
        send_title_label_     = nullptr;
        send_input_bubble_   = nullptr;
        send_input_label_    = nullptr;
        send_cursor_label_   = nullptr;
        send_status_label_   = nullptr;
        send_cancel_button_  = nullptr;
        send_confirm_button_ = nullptr;
    }
    if (deleted == page_indicator_) {
        page_indicator_ = nullptr;
        page_dots_[0]   = nullptr;
        page_dots_[1]   = nullptr;
    }
    if (active_view_ == deleted) active_view_ = nullptr;
    if (deleted == page_root_) {
        page_root_                = nullptr;
        help_view_                = nullptr;
        messages_view_            = nullptr;
        message_list_             = nullptr;
        empty_message_label_      = nullptr;
        empty_message_hint_label_ = nullptr;
        last_message_row_         = nullptr;
        info_view_                = nullptr;
        info_status_dot_          = nullptr;
        info_status_label_        = nullptr;
        info_change_name_button_  = nullptr;
        info_stats_label_         = nullptr;
        info_table_               = nullptr;
        info_table_content_       = nullptr;
        info_value_labels_.fill(nullptr);
        send_view_                = nullptr;
        send_title_label_          = nullptr;
        send_input_bubble_        = nullptr;
        send_input_label_         = nullptr;
        send_cursor_label_         = nullptr;
        send_status_label_        = nullptr;
        send_cancel_button_       = nullptr;
        send_confirm_button_      = nullptr;
        page_indicator_           = nullptr;
        page_dots_[0]             = nullptr;
        page_dots_[1]             = nullptr;
        active_view_              = nullptr;
        app_active_               = false;
        if (clipboard_notice_timer_) { lv_timer_delete(clipboard_notice_timer_); clipboard_notice_timer_ = nullptr; }
        clipboard_notice_ = ClipboardNotice::None;
        cancel_message_title_animation();
        messages_title_ = nullptr;
        if (poll_timer_) { lv_timer_delete(poll_timer_); poll_timer_ = nullptr; }
    }
    if (!ui_ready()) {
        app_active_ = false;
        if (poll_timer_) { lv_timer_delete(poll_timer_); poll_timer_ = nullptr; }
    }
}

void LoraScreen::static_owned_obj_delete_cb(lv_event_t *event) noexcept
{
    try {
        if (!event ||
            !lora_owned_delete_callback_allowed(lv_event_get_target(event), lv_event_get_current_target(event)))
            return;
        auto *self    = static_cast<LoraScreen *>(lv_event_get_user_data(event));
        auto *deleted = static_cast<lv_obj_t *>(lv_event_get_target(event));
        if (self) self->clear_deleted_handles(deleted);
    } catch (...) {
        auto *self = event ? static_cast<LoraScreen *>(lv_event_get_user_data(event)) : nullptr;
        if (self) self->app_active_ = false;
    }
}

lv_obj_t *LoraScreen::append_message_row(const LoraChatMessage &message, bool selected)
{
    if (!message_list_) return nullptr;
    char metadata[64] = "";
    if (!message.outgoing && !message.sender_name.empty())
        std::snprintf(metadata, sizeof(metadata), "From %s", message.sender_name.c_str());
    else if (!message.outgoing)
        std::snprintf(metadata, sizeof(metadata), "%.0f dBm  /  %.1f dB", message.rssi, message.snr);
    else if (message.delivery == LoraMessageDelivery::PENDING)
        std::snprintf(metadata, sizeof(metadata), "Sending");
    else if (message.delivery == LoraMessageDelivery::FAILED)
        std::snprintf(metadata, sizeof(metadata), "Failed");

    static constexpr int32_t HORIZONTAL_PADDING = 10;
    static constexpr int32_t MAX_TEXT_WIDTH     = 224;
    static constexpr int32_t MAX_BUBBLE_WIDTH   = 244;
    lv_point_t text_size{};
    lv_text_get_size(&text_size, message.text.c_str(), &lv_font_montserrat_12, 0, 0, MAX_TEXT_WIDTH, LV_TEXT_FLAG_NONE);
    int32_t content_width = text_size.x;
    std::string reply_preview;
    std::string rendered_reply_preview;
    if (!message.reply_to.empty()) {
        const std::string sender = message.reply_to_sender.empty() ? "Unknown" : message.reply_to_sender;
        reply_preview = std::string{"Reply: "} + sender + ' ' + message.reply_to;
        rendered_reply_preview = std::string{"Reply: "} + lora_app_detail::kReplyNicknameRecolorTag + sender + "# " +
                                 message.reply_to;
        lv_point_t reply_size{};
        lv_text_get_size(&reply_size, reply_preview.c_str(), &lv_font_montserrat_10, 0, 0, MAX_TEXT_WIDTH,
                         LV_TEXT_FLAG_NONE);
        content_width = std::max(content_width, reply_size.x);
    }
    if (metadata[0]) {
        lv_point_t metadata_size{};
        lv_text_get_size(&metadata_size, metadata, &lv_font_montserrat_10, 0, 0, MAX_TEXT_WIDTH, LV_TEXT_FLAG_NONE);
        content_width = std::max(content_width, metadata_size.x);
    }
    int32_t bubble_width =
        std::max<int32_t>(64, std::min<int32_t>(MAX_BUBBLE_WIDTH, content_width + HORIZONTAL_PADDING * 2));

    lv_obj_t *row = make_plain_container(message_list_, 0, 0, LV_PCT(100), LV_SIZE_CONTENT);
    if (!row) return nullptr;
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, message.outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    if (message.outgoing) lv_obj_add_flag(row, LV_OBJ_FLAG_USER_1);
    lv_obj_add_event_cb(row, bubble_tail_draw_cb, LV_EVENT_DRAW_MAIN_END, nullptr);

    uint32_t bubble_color = message.outgoing ? 0x3FCC75 : 0xCCCCCC;
    if (message.delivery == LoraMessageDelivery::PENDING) bubble_color = 0xD6B75C;
    if (message.delivery == LoraMessageDelivery::FAILED) bubble_color = 0xD96C6C;
    if (selected) bubble_color = 0xF2C94C;
    lv_obj_t *bubble = make_panel(row, 0, 0, bubble_width, LV_SIZE_CONTENT,
                                  lv_color_hex(bubble_color), LV_OPA_COVER, 8);
    if (!bubble) return row;
    lv_obj_set_style_margin_left(bubble, message.outgoing ? 0 : lora_app_detail::kBubbleTailWidth,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_margin_right(bubble, message.outgoing ? lora_app_detail::kBubbleTailWidth : 0,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_margin_bottom(bubble, lora_app_detail::kBubbleTailDrop, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bubble, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(bubble, HORIZONTAL_PADDING, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(bubble, HORIZONTAL_PADDING, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(bubble, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(bubble, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(bubble, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (!reply_preview.empty()) {
        const lv_coord_t quote_width = bubble_width - HORIZONTAL_PADDING * 2;
        const lv_coord_t quote_text_width = quote_width - 10;
        lv_point_t quote_text_size{};
        lv_text_get_size(&quote_text_size, reply_preview.c_str(), &lv_font_montserrat_10, 0, 0, quote_text_width,
                         LV_TEXT_FLAG_NONE);
        const lv_coord_t quote_height = quote_text_size.y + 6;
        const lv_color_t quote_color = message.outgoing ? lv_color_hex(0x2D6B46) : lv_color_hex(0xA9A9A9);
        lv_obj_t *quote = make_panel(bubble, 0, 0, quote_width, quote_height, quote_color, LV_OPA_COVER, 3);
        if (quote) {
            make_panel(quote, 0, 2, 3, quote_height - 4,
                       message.outgoing ? lv_color_hex(0xB9F2CE) : lv_color_hex(0x555555), LV_OPA_COVER, 1);
            lv_obj_t *reply_label =
                make_label(quote, rendered_reply_preview.c_str(), 8, 3, quote_text_width, quote_text_size.y,
                           &lv_font_montserrat_10,
                           message.outgoing ? lv_color_hex(0xE7FFF0) : lv_color_hex(0x252525), LV_TEXT_ALIGN_LEFT);
            if (reply_label) lv_label_set_recolor(reply_label, true);
        }
    }
    make_label(bubble, message.text.c_str(), 0, 0, bubble_width - HORIZONTAL_PADDING * 2, LV_SIZE_CONTENT,
               &lv_font_montserrat_12, lv_color_hex(0x000000), LV_TEXT_ALIGN_LEFT);
    if (metadata[0]) {
        std::string rendered_metadata{metadata};
        if (!message.outgoing && !message.sender_name.empty())
            rendered_metadata = std::string{"From "} + lora_app_detail::kNicknameRecolorTag + message.sender_name + '#';
        lv_obj_t *metadata_label =
            make_label(bubble, rendered_metadata.c_str(), 0, 0, bubble_width - HORIZONTAL_PADDING * 2, LV_SIZE_CONTENT,
                       &lv_font_montserrat_10, lv_color_hex(0x7E7E7E), LV_TEXT_ALIGN_RIGHT);
        if (metadata_label) lv_label_set_recolor(metadata_label, !message.outgoing && !message.sender_name.empty());
    }
    return row;
}
