#include "lora_page_model.hpp"

#include <algorithm>
#include <utility>

void LoraPageModel::reset(bool hardware_ready)
{
    view_ = hardware_ready ? LoraView::MESSAGES : LoraView::INFO;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    reply_to_.clear();
    reply_to_sender_.clear();
    tx_input_limit_ = TX_INPUT_LIMIT;
    editor_mode_ = LoraEditorMode::NONE;
    messages_.clear();
    selected_message_index_.reset();
}

void LoraPageModel::reset_after_initialization(bool hardware_ready)
{
    if (editor_mode_ != LoraEditorMode::NONE) return;
    reset(hardware_ready);
}

void LoraPageModel::begin_send(char first_character)
{
    view_ = LoraView::SEND;
    editor_mode_ = LoraEditorMode::MESSAGE;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    reply_to_.clear();
    reply_to_sender_.clear();
    tx_input_limit_ = TX_INPUT_LIMIT;
    if (first_character >= 0x20 && first_character <= 0x7e) {
        tx_input_.push_back(first_character);
        tx_cursor_ = 1;
    }
}

void LoraPageModel::begin_reply(std::string reply_to, std::string reply_to_sender)
{
    begin_send();
    reply_to_ = std::move(reply_to);
    reply_to_sender_ = std::move(reply_to_sender);
    tx_input_limit_ = lora_chat_protocol::kMaxReplyMessageBytes;
}

void LoraPageModel::begin_nickname_edit()
{
    view_        = LoraView::INFO;
    editor_mode_ = LoraEditorMode::NICKNAME;
    tx_input_    = nickname_;
    tx_cursor_   = tx_input_.size();
    send_status_.clear();
    reply_to_.clear();
    reply_to_sender_.clear();
    tx_input_limit_ = TX_INPUT_LIMIT;
}

void LoraPageModel::cancel_editor()
{
    view_ = editor_mode_ == LoraEditorMode::NICKNAME ? LoraView::INFO : LoraView::MESSAGES;
    editor_mode_ = LoraEditorMode::NONE;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    reply_to_.clear();
    reply_to_sender_.clear();
    tx_input_limit_ = TX_INPUT_LIMIT;
}

bool LoraPageModel::append_character(char character)
{
    if (character < 0x20 || character > 0x7e) return false;
    const size_t limit = editor_mode_ == LoraEditorMode::NICKNAME ? lora_chat_protocol::kMaxNicknameBytes
                                                                  : tx_input_limit_;
    if (tx_input_.size() >= limit) {
        send_status_ = editor_mode_ == LoraEditorMode::NICKNAME ? "10 byte limit" : "Message is too long";
        return false;
    }
    tx_input_.insert(tx_cursor_, 1, character);
    ++tx_cursor_;
    send_status_.clear();
    return true;
}

bool LoraPageModel::insert_text(std::string_view text)
{
    if (text.empty() ||
        !std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return character >= 0x20 && character <= 0x7e;
        }))
        return false;
    const size_t limit = editor_mode_ == LoraEditorMode::NICKNAME ? lora_chat_protocol::kMaxNicknameBytes
                                                                  : tx_input_limit_;
    if (tx_input_.size() >= limit) {
        send_status_ = editor_mode_ == LoraEditorMode::NICKNAME ? "10 byte limit" : "Message is too long";
        return false;
    }
    const size_t inserted_size = std::min(text.size(), limit - tx_input_.size());
    tx_input_.insert(tx_cursor_, text.substr(0, inserted_size));
    tx_cursor_ += inserted_size;
    send_status_.clear();
    return true;
}

bool LoraPageModel::erase_character()
{
    if (tx_cursor_ == 0 || tx_input_.empty()) return false;
    tx_input_.erase(tx_cursor_ - 1, 1);
    --tx_cursor_;
    send_status_.clear();
    return true;
}

bool LoraPageModel::move_cursor(int offset)
{
    if (offset == 0 || (offset < 0 ? tx_cursor_ == 0 : tx_cursor_ >= tx_input_.size())) return false;
    tx_cursor_ += offset;
    send_status_.clear();
    return true;
}

void LoraPageModel::set_cursor_position(size_t position)
{
    tx_cursor_ = position < tx_input_.size() ? position : tx_input_.size();
    send_status_.clear();
}

void LoraPageModel::complete_send()
{
    view_ = LoraView::MESSAGES;
    editor_mode_ = LoraEditorMode::NONE;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    reply_to_.clear();
    reply_to_sender_.clear();
    tx_input_limit_ = TX_INPUT_LIMIT;
}

void LoraPageModel::complete_nickname_edit(std::string nickname)
{
    nickname_    = std::move(nickname);
    view_        = LoraView::INFO;
    editor_mode_ = LoraEditorMode::NONE;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
}

void LoraPageModel::append_message(std::string text, bool outgoing, float rssi, float snr, std::string sender_name,
                                   LoraMessageDelivery delivery, std::string reply_to, std::string reply_to_sender)
{
    if (text.empty()) text = "<empty>";
    if (messages_.size() >= MESSAGE_HISTORY_LIMIT) {
        messages_.pop_front();
        if (selected_message_index_) {
            if (*selected_message_index_ == 0)
                selected_message_index_.reset();
            else
                --*selected_message_index_;
        }
    }
    messages_.push_back({std::move(text), outgoing, rssi, snr, std::move(sender_name), delivery,
                         std::move(reply_to), std::move(reply_to_sender)});
}

bool LoraPageModel::resolve_latest_pending(bool sent)
{
    for (auto message = messages_.rbegin(); message != messages_.rend(); ++message) {
        if (!message->outgoing || message->delivery != LoraMessageDelivery::PENDING) continue;
        message->delivery = sent ? LoraMessageDelivery::SENT : LoraMessageDelivery::FAILED;
        return true;
    }
    return false;
}

bool LoraPageModel::select_message(int direction)
{
    if (messages_.empty()) return false;
    const auto previous = selected_message_index_;
    if (!selected_message_index_) {
        selected_message_index_ = messages_.size() - 1;
    } else if (direction < 0 && *selected_message_index_ > 0)
        --*selected_message_index_;
    else if (direction > 0 && *selected_message_index_ + 1 < messages_.size())
        ++*selected_message_index_;
    return previous != selected_message_index_;
}

bool LoraPageModel::clear_message_selection()
{
    if (!selected_message_index_) return false;
    selected_message_index_.reset();
    return true;
}

const LoraChatMessage *LoraPageModel::selected_message() const
{
    if (!selected_message_index_ || *selected_message_index_ >= messages_.size()) return nullptr;
    return &messages_[*selected_message_index_];
}

const LoraChatMessage *LoraPageModel::find_message(uint32_t id) const
{
    for (auto message = messages_.rbegin(); message != messages_.rend(); ++message)
        if (lora_chat_protocol::message_id(message->text) == id) return &*message;
    return nullptr;
}
