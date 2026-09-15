#include "lora_page_model.hpp"

#include <utility>

void LoraPageModel::reset(bool hardware_ready)
{
    view_ = hardware_ready ? LoraView::MESSAGES : LoraView::INFO;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    messages_.clear();
}

void LoraPageModel::begin_send(char first_character)
{
    view_ = LoraView::SEND;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
    if (first_character >= 0x20 && first_character <= 0x7e) {
        tx_input_.push_back(first_character);
        tx_cursor_ = 1;
    }
}

void LoraPageModel::cancel_send()
{
    view_ = LoraView::MESSAGES;
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
}

bool LoraPageModel::append_character(char character)
{
    if (character < 0x20 || character > 0x7e || tx_input_.size() >= TX_INPUT_LIMIT) return false;
    tx_input_.insert(tx_cursor_, 1, character);
    ++tx_cursor_;
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
    tx_input_.clear();
    tx_cursor_ = 0;
    send_status_.clear();
}

void LoraPageModel::append_message(std::string text, bool outgoing, float rssi, float snr,
                                   LoraMessageDelivery delivery)
{
    if (text.empty()) text = "<empty>";
    if (messages_.size() >= MESSAGE_HISTORY_LIMIT) messages_.pop_front();
    messages_.push_back({std::move(text), outgoing, rssi, snr, delivery});
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
