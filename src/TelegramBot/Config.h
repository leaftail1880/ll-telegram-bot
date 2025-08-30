#pragma once

#include <string>
#include <vector>

#define VERSION 15

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

    std::vector<std::string> blacklist{"tp", "summon adv:biome ^^^-1.2", "tell", "w", "msg", "broadcastchatmessage"};
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

struct CustomCommandConfig {
    std::string name;
    std::string description;
    std::string command;
    bool        adminOnly = true;
};

struct CustomCommands {
    std::vector<CustomCommandConfig> commands;
    std::string                      langCode = "en_US";

    bool includeCommandInResponse = true;
};

struct LogSource {
    std::string folder     = "logs";
    std::string fileFormat = "player_actions_{year}-{month}-{day}.csv";

    std::string columnDelimeter = ",";
    size_t      nameColumnIndex = 2;
    size_t      maxPages        = 15;
};

struct LogsSearchCommand {
    bool                   enabled = false;
    std::vector<LogSource> logSources{{}};
};

struct Config {
    int          version          = VERSION;
    std::string  telegramBotToken = "INSERT YOUR TOKEN HERE";
    std::int64_t telegramChatId   = 0;
    std::int32_t telegramTopicId  = -1;

    std::int64_t telegramAdminChatId = 0;

    long telegramTimeoutSec        = 5;
    long telegramPollingTimeoutSec = 4;
    int  telegramPollingLimit      = 100;

    bool telegramIgnoreCommands   = true;
    bool telegramIgnoreOtherBots  = true;
    bool telegramIgnoreOtherChats = true;

    std::string telegramStartMessage = "Bot started\\!";
    std::string telegramStopMessage  = R"(Bot stopped\.\.\.)";

    std::string minecraftGlobalChatPrefix{};

    ConfigChatSource minecraft{.sourceName = "Minecraft", .joinTextFormat = "", .leaveTextFormat = ""};
    ConfigChatSource telegram{
        .sourceName          = "Telegram",
        .chatFormat          = "**{{username}}:** {{message}}",
        .consoleLogFormat    = "{{sourceName}} {{name}}: {{message}}",
        .clearFromColorCodes = true,
    };

    LogsSearchCommand logsSearchCommand;

    CommandLogsConfig commandLogs;

    CustomCommands customCommands{
        .commands{
                  {.name = "allow", .description = "Adds player to allowlist", .command = "allowlist add $1"},
                  {.name = "disallow", .description = "Removes player from allowlist", .command = "allowlist remove $1"}
        }
    };
};

extern Config config;

} // namespace telegram_bot
