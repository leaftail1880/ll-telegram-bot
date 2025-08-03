#include "TelegramBot/Config.h"
#pragma once

namespace telegram_bot {

class Utils final {
public:
    static std::string replacePlaceholders(
        const std::string&      format,
        const ConfigChatSource& chatSource,
        const PlaceholderData&  placeholders
    );

    static std::string replaceKillPlaceholders(
        const std::string&         format,
        const ConfigChatSource&    chatSource,
        const KillPlaceholderData& placeholders
    );

    static void replaceString(std::string& subject, const std::string& search, const std::string& replace);

    static void broadcast(std::string_view message);
};

} // namespace telegram_bot