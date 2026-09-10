#include "models/lora_page_model.hpp"
#include "models/lora_page_contract.hpp"
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
    model.cancel_send();
    CHECK(model.view() == LoraView::MESSAGES);
    CHECK(model.tx_input().empty());

    model.begin_send();
    for (size_t index = 0; index < LoraPageModel::TX_INPUT_LIMIT; ++index)
        CHECK(model.append_character('x'));
    CHECK(!model.append_character('y'));
    CHECK(model.tx_input().size() == LoraPageModel::TX_INPUT_LIMIT);

    model.set_send_status("sending");
    model.complete_send();
    CHECK(model.view() == LoraView::MESSAGES);
    CHECK(model.tx_input().empty());
    CHECK(model.send_status().empty());

    model.append_message("", false, -81.0f, 7.5f);
    CHECK(model.messages().back().text == "<empty>");
    model.append_message("pending", true, 0.0f, 0.0f, LoraMessageDelivery::PENDING);
    CHECK(model.messages().back().delivery == LoraMessageDelivery::PENDING);
    CHECK(model.resolve_latest_pending(true));
    CHECK(model.messages().back().delivery == LoraMessageDelivery::SENT);
    CHECK(!model.resolve_latest_pending(false));
    model.append_message("failed", true, 0.0f, 0.0f, LoraMessageDelivery::PENDING);
    CHECK(model.resolve_latest_pending(false));
    CHECK(model.messages().back().delivery == LoraMessageDelivery::FAILED);
    for (size_t index = 0; index < LoraPageModel::MESSAGE_HISTORY_LIMIT + 3; ++index)
        model.append_message(std::to_string(index), true, 0.0f, 0.0f);
    CHECK(model.messages().size() == LoraPageModel::MESSAGE_HISTORY_LIMIT);
    CHECK(model.messages().front().text == "3");
    CHECK(model.messages().back().text == "66");
}
