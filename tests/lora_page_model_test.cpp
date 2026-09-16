#include "models/lora_page_model.hpp"
#include "models/lora_page_contract.hpp"
#include "models/lora_chat_protocol.hpp"
#include "test_support.hpp"
#include "views/lora_screen.hpp"

#include <string>

int main()
{
    static_assert(noexcept(lora_poll_callback_allowed(true, true)));
    static_assert(lora_poll_callback_allowed(true, true));
    static_assert(!lora_poll_callback_allowed(false, true));
    static_assert(!lora_poll_callback_allowed(true, false));

    int animation_target = 1;
    static_assert(noexcept(lora_animation_callback_allowed(&animation_target)));
    CHECK(lora_animation_callback_allowed(&animation_target));
    CHECK(!lora_animation_callback_allowed(static_cast<int *>(nullptr)));

    int delete_target = 1;
    int bubbled_target = 2;
    CHECK(lora_owned_delete_callback_allowed(&delete_target, &delete_target));
    CHECK(!lora_owned_delete_callback_allowed(&delete_target, &bubbled_target));
    CHECK(!lora_owned_delete_callback_allowed(
        static_cast<int *>(nullptr), &delete_target));

    int action_button = 3;
    int stale_button = 4;
    static_assert(noexcept(lora_send_action_callback_allowed(
        static_cast<int *>(nullptr), static_cast<int *>(nullptr), false, false)));
    CHECK(lora_send_action_callback_allowed(
        &action_button, &action_button, true, true));
    CHECK(!lora_send_action_callback_allowed(
        &stale_button, &action_button, true, true));
    CHECK(!lora_send_action_callback_allowed(
        &action_button, &action_button, false, true));
    CHECK(!lora_send_action_callback_allowed(
        &action_button, &action_button, true, false));

    int page_root = 5;
    int stale_page = 6;
    static_assert(noexcept(lora_page_event_callback_allowed(
        static_cast<int *>(nullptr), static_cast<int *>(nullptr), false)));
    CHECK(lora_page_event_callback_allowed(&page_root, &page_root, true));
    CHECK(!lora_page_event_callback_allowed(&stale_page, &page_root, true));
    CHECK(!lora_page_event_callback_allowed(&page_root, &page_root, false));

    CHECK(lora_info_response_valid(0, 32, 32));
    CHECK(!lora_info_response_valid(-1, 32, 32));
    CHECK(!lora_info_response_valid(0, 31, 32));

    int handle = 1;
    int *present = &handle;
    int *missing = nullptr;
    CHECK(lora_page_ui_ready(present, present, present, present, present, present));
    CHECK(!lora_page_ui_ready(missing, present, present, present, present, present));
    CHECK(!lora_page_ui_ready(present, present, missing, present, present, present));
    CHECK(!lora_page_ui_ready(present, present, present, present, present, missing));
    CHECK(lora_page_controls_ready(present, present, present));
    CHECK(!lora_page_controls_ready(present, missing, present));

    LoraPageModel model;
    model.reset(true);
    CHECK(model.view() == LoraView::MESSAGES);
    CHECK(model.messages().empty());

    CHECK(lora_app_detail::normalize_lora_key('f', LoraView::MESSAGES) == LV_KEY_UP);
    CHECK(lora_app_detail::normalize_lora_key('F', LoraView::MESSAGES) == LV_KEY_UP);
    CHECK(lora_app_detail::normalize_lora_key('x', LoraView::MESSAGES) == LV_KEY_DOWN);
    CHECK(lora_app_detail::normalize_lora_key('X', LoraView::INFO) == LV_KEY_DOWN);
    CHECK(lora_app_detail::normalize_lora_key('f', LoraView::INFO) == LV_KEY_UP);
    CHECK(lora_app_detail::normalize_lora_key('f', LoraView::SEND) == 'f');
    CHECK(lora_app_detail::normalize_lora_key('X', LoraView::SEND) == 'X');

    model.reset(false);
    CHECK(model.view() == LoraView::INFO);

    model.begin_send('A');
    CHECK(model.view() == LoraView::SEND);
    CHECK(model.tx_input() == "A");
    CHECK(!model.append_character('\n'));
    CHECK(model.tx_input() == "A");
    CHECK(model.append_character('B'));
    CHECK(model.erase_character());
    CHECK(model.tx_input() == "A");
    model.cancel_editor();
    CHECK(model.view() == LoraView::MESSAGES);
    CHECK(model.tx_input().empty());

    model.set_nickname("Alice");
    model.begin_nickname_edit();
    CHECK(model.view() == LoraView::INFO);
    CHECK(model.editor_mode() == LoraEditorMode::NICKNAME);
    CHECK(model.tx_input() == "Alice");
    while (model.tx_input().size() < lora_chat_protocol::kMaxNicknameBytes) CHECK(model.append_character('x'));
    const std::string maximum_nickname = model.tx_input();
    CHECK(!model.append_character('y'));
    CHECK(model.tx_input() == maximum_nickname);
    CHECK(model.send_status() == "10 byte limit");
    model.complete_nickname_edit(model.tx_input());
    CHECK(model.nickname() == maximum_nickname);

    model.begin_send();
    for (size_t index = 0; index < LoraPageModel::TX_INPUT_LIMIT; ++index)
        CHECK(model.append_character('x'));
    const std::string maximum_input = model.tx_input();
    CHECK(!model.append_character('y'));
    CHECK(model.tx_input() == maximum_input);
    CHECK(model.send_status() == "Message is too long");
    model.cancel_editor();
    model.begin_send();
    CHECK(model.insert_text("copied"));
    CHECK(model.tx_input() == "copied");
    const std::string oversized_paste(LoraPageModel::TX_INPUT_LIMIT, 'p');
    CHECK(model.insert_text(oversized_paste));
    CHECK(model.tx_input().size() == LoraPageModel::TX_INPUT_LIMIT);
    CHECK(model.tx_input().substr(0, 6) == "copied");
    CHECK(model.send_status().empty());
    model.cancel_editor();
    model.begin_send();
    const std::string maximum_paste(LoraPageModel::TX_INPUT_LIMIT, 'p');
    CHECK(model.insert_text(maximum_paste));
    CHECK(model.tx_input() == maximum_paste);
    CHECK(!model.insert_text("p"));
    CHECK(model.tx_input() == maximum_paste);

    const std::string quoted_message(LoraPageModel::TX_INPUT_LIMIT, 'q');
    const std::string maximum_reply(lora_chat_protocol::kMaxReplyMessageBytes, 'r');
    model.begin_reply(quoted_message, "Alice");
    CHECK(model.reply_to() == quoted_message);
    CHECK(model.reply_to_sender() == "Alice");
    CHECK(model.insert_text(maximum_input));
    CHECK(model.reply_to() == quoted_message);
    CHECK(model.reply_to_sender() == "Alice");
    CHECK(model.tx_input() == maximum_input.substr(0, lora_chat_protocol::kMaxReplyMessageBytes));
    CHECK(!model.append_character('x'));
    CHECK(model.tx_input() == maximum_input.substr(0, lora_chat_protocol::kMaxReplyMessageBytes));
    CHECK(model.send_status() == "Message is too long");

    model.set_send_status("sending");
    model.complete_send();
    CHECK(model.view() == LoraView::MESSAGES);
    CHECK(model.tx_input().empty());
    CHECK(model.send_status().empty());
    CHECK(model.reply_to().empty());
    CHECK(model.reply_to_sender().empty());

    model.append_message("", false, -81.0f, 7.5f);
    CHECK(model.messages().back().text == "<empty>");
    model.append_message("pending", true, 0.0f, 0.0f, {}, LoraMessageDelivery::PENDING);
    CHECK(model.messages().back().delivery == LoraMessageDelivery::PENDING);
    CHECK(model.resolve_latest_pending(true));
    CHECK(model.messages().back().delivery == LoraMessageDelivery::SENT);
    CHECK(!model.resolve_latest_pending(false));
    model.append_message("failed", true, 0.0f, 0.0f, {}, LoraMessageDelivery::PENDING);
    CHECK(model.resolve_latest_pending(false));
    CHECK(model.messages().back().delivery == LoraMessageDelivery::FAILED);
    for (size_t index = 0; index < LoraPageModel::MESSAGE_HISTORY_LIMIT + 3; ++index)
        model.append_message(std::to_string(index), true, 0.0f, 0.0f);
    CHECK(model.messages().size() == LoraPageModel::MESSAGE_HISTORY_LIMIT);
    CHECK(model.messages().front().text == "3");
    CHECK(model.messages().back().text == "66");
    CHECK(model.select_message(-1));
    CHECK(model.selected_message() && model.selected_message()->text == "66");
    CHECK(model.select_message(-1));
    CHECK(model.selected_message() && model.selected_message()->text == "65");
    CHECK(model.select_message(1));
    CHECK(model.selected_message() && model.selected_message()->text == "66");
    model.append_message("67", false, -70.0f, 8.0f, {}, LoraMessageDelivery::RECEIVED,
                         quoted_message, "Alice");
    CHECK(model.messages().back().reply_to == quoted_message);
    CHECK(model.messages().back().reply_to_sender == "Alice");
    CHECK(model.find_message(lora_chat_protocol::message_id("67")) == &model.messages().back());
    CHECK(model.selected_message() && model.selected_message()->text == "66");
    CHECK(model.clear_message_selection());
    CHECK(!model.selected_message());
    CHECK(!model.clear_message_selection());

    LoraPageModel delivery_model;
    delivery_model.reset(true);
    delivery_model.append_message("sending", true, 0.0f, 0.0f, {}, LoraMessageDelivery::PENDING);
    CHECK(delivery_model.select_message(-1));
    CHECK(delivery_model.selected_message() && delivery_model.selected_message()->text == "sending");
    delivery_model.append_message("failed", true, 0.0f, 0.0f, {}, LoraMessageDelivery::FAILED);
    CHECK(delivery_model.clear_message_selection());
    CHECK(delivery_model.select_message(-1));
    CHECK(delivery_model.selected_message() && delivery_model.selected_message()->text == "failed");
    delivery_model.append_message("received", false, -70.0f, 8.0f);
    delivery_model.append_message("sending 2", true, 0.0f, 0.0f, {}, LoraMessageDelivery::PENDING);
    delivery_model.append_message("sent", true, 0.0f, 0.0f, {}, LoraMessageDelivery::SENT);
    CHECK(delivery_model.clear_message_selection());
    CHECK(delivery_model.select_message(-1));
    CHECK(delivery_model.selected_message() && delivery_model.selected_message()->text == "sent");
    CHECK(delivery_model.select_message(-1));
    CHECK(delivery_model.selected_message() && delivery_model.selected_message()->text == "sending 2");
    CHECK(delivery_model.select_message(1));
    CHECK(delivery_model.selected_message() && delivery_model.selected_message()->text == "sent");

    const std::string named = lora_chat_protocol::encode("hello", "Peer");
    CHECK(named.size() == 10);
    const auto decoded_named = lora_chat_protocol::decode(named);
    CHECK(decoded_named.message == "hello");
    CHECK(decoded_named.nickname == "Peer");
    const auto decoded_legacy = lora_chat_protocol::decode("legacy");
    CHECK(decoded_legacy.message == "legacy");
    CHECK(decoded_legacy.nickname.empty());
    const std::string maximum_message(lora_chat_protocol::kMaxMessageBytes, 'm');
    const std::string maximum_name(lora_chat_protocol::kMaxNicknameBytes, 'n');
    const auto decoded_maximum = lora_chat_protocol::decode(lora_chat_protocol::encode(maximum_message, maximum_name));
    CHECK(decoded_maximum.message == maximum_message);
    CHECK(decoded_maximum.nickname == maximum_name);
    CHECK(!decoded_maximum.has_reply);

    const std::string maximum_reply_payload =
        lora_chat_protocol::encode(maximum_reply, maximum_name, maximum_message);
    CHECK(maximum_reply_payload.size() == lora_chat_protocol::kRadioPayloadBytes);
    const auto decoded_reply = lora_chat_protocol::decode(maximum_reply_payload);
    CHECK(decoded_reply.message == maximum_reply);
    CHECK(decoded_reply.nickname == maximum_name);
    CHECK(decoded_reply.has_reply);
    CHECK(decoded_reply.reply_id == lora_chat_protocol::message_id(maximum_message));
    CHECK(lora_chat_protocol::encode(std::string(lora_chat_protocol::kMaxReplyMessageBytes + 1, 'x'),
                                     maximum_name, maximum_message).empty());
}
