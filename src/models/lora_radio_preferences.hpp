#pragma once

#include "lora/lora_radio_settings.hpp"

#include <string>

namespace cap_lora {

struct RadioPreferences {
    bool skip_notice = false;
    RadioSettings settings;
};

RadioPreferences load_radio_preferences();
bool save_radio_preferences(const RadioPreferences& preferences, std::string& error);

}  // namespace cap_lora
