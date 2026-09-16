#pragma once

#include <string>

namespace lora_nickname_store {

std::string load_or_default();
bool save(const std::string& nickname, std::string& error);

}  // namespace lora_nickname_store
