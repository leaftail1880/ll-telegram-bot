#include "TelegramBot/Config.h"
#pragma once

namespace telegram_bot {

class Utils final {
public:
    static std::string replacePlaceholders(
        const std::string&      format,
        const ConfigChatSource& chatSource,
        const PlaceholderData&  placeholders,
        bool                    escapeForTelegram = false
    );

    static std::string replaceKillPlaceholders(
        const std::string&         format,
        const ConfigChatSource&    chatSource,
        const KillPlaceholderData& placeholders,
        bool                       escapeForTelegram = false
    );

    static void replaceString(std::string& subject, const std::string& search, const std::string& replace);

    static void broadcast(std::string_view message);

    static std::string escapeStringForTelegram(const std::string& input);
};

} // namespace telegram_bot