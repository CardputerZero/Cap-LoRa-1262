#include "models/lora_nickname_store.hpp"

#include "models/lora_chat_protocol.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace lora_nickname_store {
namespace {

constexpr char kConfigDirectoryName[] = "M5CardputerZero-Cap-LoRa-1262";
constexpr char kNicknameFileName[]     = "nickname";

struct UserConfigLocation {
    std::filesystem::path directory;
    uid_t uid = 0;
    gid_t gid = 0;
};

UserConfigLocation user_config_location()
{
    const char* sudo_user = std::getenv("SUDO_USER");
    const passwd* account = sudo_user && sudo_user[0] != '\0' ? getpwnam(sudo_user) : getpwuid(geteuid());
    if (account && account->pw_dir && account->pw_dir[0] != '\0') {
        return {std::filesystem::path(account->pw_dir) / ".config" / kConfigDirectoryName, account->pw_uid,
                account->pw_gid};
    }
    const char* home = std::getenv("HOME");
    return {std::filesystem::path(home && home[0] != '\0' ? home : ".") / ".config" / kConfigDirectoryName,
            geteuid(), getegid()};
}

bool valid_nickname(const std::string& nickname)
{
    return !nickname.empty() && nickname.size() <= lora_chat_protocol::kMaxNicknameBytes &&
           nickname.find_first_not_of(' ') != std::string::npos && lora_chat_protocol::is_printable_ascii(nickname);
}

std::filesystem::path nickname_path()
{
    return user_config_location().directory / kNicknameFileName;
}

std::string current_user_name()
{
    const char* username = std::getenv("SUDO_USER");
    if (!username || username[0] == '\0') {
        const passwd* account = getpwuid(geteuid());
        username = account && account->pw_name && account->pw_name[0] != '\0' ? account->pw_name : "LoRa";
    }
    std::string nickname{username};
    nickname.resize(std::min(nickname.size(), lora_chat_protocol::kMaxNicknameBytes));
    return nickname;
}

}  // namespace

std::string load_or_default()
{
    const auto path = nickname_path();
    std::ifstream input(path);
    std::string nickname;
    if (input.is_open()) std::getline(input, nickname);
    if (valid_nickname(nickname)) {
        spdlog::info("LoRa nickname: loaded from {}", path.string());
        return nickname;
    }
    return current_user_name();
}

bool save(const std::string& nickname, std::string& error)
{
    if (!valid_nickname(nickname)) {
        error = "invalid nickname";
        return false;
    }

    const auto location = user_config_location();
    const auto path     = location.directory / kNicknameFileName;
    std::error_code filesystem_error;
    std::filesystem::create_directories(location.directory, filesystem_error);
    if (filesystem_error) {
        error = "cannot create " + location.directory.string();
        return false;
    }
    const auto directory_status = std::filesystem::symlink_status(location.directory, filesystem_error);
    if (filesystem_error || !std::filesystem::is_directory(directory_status) ||
        std::filesystem::is_symlink(directory_status)) {
        error = "invalid configuration directory";
        return false;
    }
    if (geteuid() == 0 && chown(location.directory.c_str(), location.uid, location.gid) != 0) {
        error = "cannot set configuration ownership: " + std::string(std::strerror(errno));
        return false;
    }

    std::string temporary_pattern = path.string() + ".tmp.XXXXXX";
    std::vector<char> temporary_buffer(temporary_pattern.begin(), temporary_pattern.end());
    temporary_buffer.push_back('\0');
    const int temporary_fd = mkstemp(temporary_buffer.data());
    if (temporary_fd < 0) {
        error = "cannot create nickname file: " + std::string(std::strerror(errno));
        return false;
    }
    const std::filesystem::path temporary{temporary_buffer.data()};
    const auto discard_temporary = [&]() {
        close(temporary_fd);
        std::filesystem::remove(temporary, filesystem_error);
    };

    std::size_t offset = 0;
    while (offset < nickname.size()) {
        const ssize_t written = write(temporary_fd, nickname.data() + offset, nickname.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            error = "cannot write nickname: " + std::string(std::strerror(errno));
            discard_temporary();
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    if ((geteuid() == 0 && fchown(temporary_fd, location.uid, location.gid) != 0) ||
        fchmod(temporary_fd, S_IRUSR | S_IWUSR) != 0 || fsync(temporary_fd) != 0) {
        error = "cannot finalize nickname: " + std::string(std::strerror(errno));
        discard_temporary();
        return false;
    }
    if (close(temporary_fd) != 0) {
        error = "cannot close nickname: " + std::string(std::strerror(errno));
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        error = "cannot replace " + path.string();
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    spdlog::info("LoRa nickname: saved to {}", path.string());
    return true;
}

}  // namespace lora_nickname_store
