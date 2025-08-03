#pragma once

#include <string>

namespace telegram_bot {

struct ConfigChatSource {
    std::string sourceName;

    std::string chatFormat = "{{sourceName}} {{username}}: {{message}}";

    std::string consoleLogFormat = "{{username}}: {{message}}";

    std::string deathTextFormat = "{{translated}}";

    std::string deathTextLogFormat = "{{killer}} killed {{deadMobOrPlayer}} cause={{cause}}";

    std::string joinTextFormat = "+{{username}}";

    std::string leaveTextFormat = "-{{username}}";

    std::string langCode = "en_US";
};

struct PlaceholderData {
    std::string username;
    std::string message;
};

struct KillPlaceholderData {
    std::string deadMobOrPlayer{};
    std::string killer{};
    std::string translated{};
    std::string cause;
};

struct Config {
    int          version          = 5;
    std::string  telegramBotToken = "INSERT YOUR TOKEN HERE";
    std::int64_t telegramChatId   = 0;
    std::int32_t telegramTopicId  = -1;

    int telegramPollingTimeoutSec = 5;

    bool telegramIgnoreCommands   = true;
    bool telegramIgnoreOtherBots  = true;
    bool telegramIgnoreOtherChats = true;

    std::string minecraftGlobalChatPrefix{};

    ConfigChatSource minecraft{.sourceName = "Minecraft", .joinTextFormat = "", .leaveTextFormat = ""};
    ConfigChatSource telegram{.sourceName = "Telegram", .consoleLogFormat = "{{sourceName}} {{username}}: {{message}}"};
};

extern Config config;


} // namespace telegram_bot
