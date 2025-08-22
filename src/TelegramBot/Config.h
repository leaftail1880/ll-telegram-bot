#pragma once

#include <string>
#include <vector>

namespace telegram_bot {

struct ConfigChatSource {
    std::string sourceName;

    std::string chatFormat       = "{{sourceName}} {{username}}: {{message}}";
    std::string consoleLogFormat = "{{username}}: {{message}}";

    std::string deathTextFormat    = "{{translated}}";
    std::string deathTextLogFormat = "{{killer}} killed {{deadMobOrPlayer}} cause={{cause}}";

    std::string joinTextFormat  = "+{{username}}";
    std::string leaveTextFormat = "-{{username}}";

    std::string langCode = "en_US";

    bool clearFromColorCodes = false;
};

struct PlaceholderData {
    std::string username;
    std::string name;
    std::string message;
};

struct KillPlaceholderData {
    std::string deadMobOrPlayer{};
    std::string killer{};
    std::string translated{};
    std::string cause;
};

struct CommandEmergencyLogConfig {
    std::string              commandStartsWith;
    std::vector<std::string> blacklist;
    std::string              message;
};

struct CommandLogsConfig {
    bool enabled = false;

    std::int64_t telegramChatId  = 0;
    std::int32_t telegramTopicId = -1;

    std::vector<std::string> blacklist{"tp", "summon adv:biome ^^^-1.2"};
    std::vector<std::string> whitelist{};
    bool                     whiteListEnabled = false;

    std::string message = "**{{username}}:** `{{message}}`";

    std::vector<CommandEmergencyLogConfig> emergency = {
        {
         .commandStartsWith = "gamemode",
         .blacklist         = {"gamemode spectator", "gamemode s"},
         .message           = "**WARNING\\!**\n{{username}} used `{{message}}` @admin",
         }
    };
};


struct Config {
    int          version          = 8;
    std::string  telegramBotToken = "INSERT YOUR TOKEN HERE";
    std::int64_t telegramChatId   = 0;
    std::int32_t telegramTopicId  = -1;

    int telegramPollingTimeoutSec = 5;

    bool telegramIgnoreCommands   = true;
    bool telegramIgnoreOtherBots  = true;
    bool telegramIgnoreOtherChats = true;

    std::string telegramStartMessage = "Bot started!";

    std::string minecraftGlobalChatPrefix{};

    ConfigChatSource minecraft{.sourceName = "Minecraft", .joinTextFormat = "", .leaveTextFormat = ""};
    ConfigChatSource telegram{
        .sourceName          = "Telegram",
        .chatFormat          = "**{{username}}:** {{message}}",
        .consoleLogFormat    = "{{sourceName}} {{name}}: {{message}}",
        .clearFromColorCodes = true,
    };

    CommandLogsConfig commandLogs;
};

extern Config config;


} // namespace telegram_bot
