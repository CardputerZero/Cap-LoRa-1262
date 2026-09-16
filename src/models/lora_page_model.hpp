#pragma once

#include "models/lora_chat_protocol.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

enum class LoraView { MESSAGES, INFO, SEND };
enum class LoraMessageDelivery { RECEIVED, PENDING, SENT, FAILED };
enum class LoraEditorMode { NONE, MESSAGE, NICKNAME };

struct LoraChatMessage {
    std::string text;
    bool outgoing = false;
    float rssi = 0.0f;
    float snr = 0.0f;
    std::string sender_name;
    LoraMessageDelivery delivery = LoraMessageDelivery::RECEIVED;
    std::string reply_to;
    std::string reply_to_sender;
};

class LoraPageModel
{
public:
    static constexpr size_t MESSAGE_HISTORY_LIMIT = 64;
    static constexpr size_t TX_INPUT_LIMIT = lora_chat_protocol::kMaxMessageBytes;

    void reset(bool hardware_ready);
    void set_view(LoraView view) { view_ = view; }
    LoraView view() const { return view_; }

    void begin_send(char first_character = 0);
    void begin_reply(std::string reply_to, std::string reply_to_sender);
    void begin_nickname_edit();
    void cancel_editor();
    bool append_character(char character);
    bool insert_text(std::string_view text);
    bool erase_character();
    bool move_cursor(int offset);
    void set_cursor_position(size_t position);
    void set_send_status(std::string status) { send_status_ = std::move(status); }
    void complete_send();
    void complete_nickname_edit(std::string nickname);

    void append_message(std::string text, bool outgoing, float rssi, float snr, std::string sender_name = {},
                        LoraMessageDelivery delivery = LoraMessageDelivery::RECEIVED, std::string reply_to = {},
                        std::string reply_to_sender = {});
    bool resolve_latest_pending(bool sent);
    bool select_message(int direction);
    bool clear_message_selection();

    const std::string &tx_input() const { return tx_input_; }
    size_t cursor_position() const { return tx_cursor_; }
    const std::string &send_status() const { return send_status_; }
    const std::string &reply_to() const { return reply_to_; }
    const std::string &reply_to_sender() const { return reply_to_sender_; }
    const std::string &nickname() const { return nickname_; }
    void set_nickname(std::string nickname) { nickname_ = std::move(nickname); }
    LoraEditorMode editor_mode() const { return editor_mode_; }
    const std::deque<LoraChatMessage> &messages() const { return messages_; }
    const std::optional<size_t> &selected_message_index() const { return selected_message_index_; }
    const LoraChatMessage *selected_message() const;
    const LoraChatMessage *find_message(uint32_t message_id) const;

private:
    LoraView view_ = LoraView::MESSAGES;
    std::string tx_input_;
    size_t tx_cursor_ = 0;
    std::string send_status_;
    std::string reply_to_;
    std::string reply_to_sender_;
    size_t tx_input_limit_ = TX_INPUT_LIMIT;
    std::string nickname_{"LoRa"};
    LoraEditorMode editor_mode_ = LoraEditorMode::NONE;
    std::deque<LoraChatMessage> messages_;
    std::optional<size_t> selected_message_index_;
};
