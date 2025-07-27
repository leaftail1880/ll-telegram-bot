#include "TelegramBot/TelegramBot.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/mod/RegisterHelper.h"
#include <ll/api/Config.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/network/PacketSender.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/packet/SetLocalPlayerAsInitializedPacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/Command.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>
#include <tgbot/tgbot.h>
#include <utility>


struct ConfigChatSource {
    std::string sourceName;

    bool        logEnabled = true;
    std::string userFormat = "{{sourceName}} {{username}}: {{message}}";

    bool        userEnabled = true;
    std::string logFormat   = "{{username}}: {{message}}";
};

struct PlaceholderData {
    std::string username;
    std::string message;
};

struct Config {
    int          version          = 1;
    std::string  telegramBotToken = "INSERT YOUR TOKEN HERE";
    std::int64_t telegramChatId   = 0;

    int telegramPollingTimeoutSec = 5;

    bool telegramIgnoreCommands   = true;
    bool telegramIgnoreOtherBots  = true;
    bool telegramIgnoreOtherChats = true;

    std::string joinTextFormat  = "+{{username}}";
    std::string leaveTextFormat = "-{{username}}";

    ConfigChatSource minecraft{.sourceName = "Minecraft"};
    ConfigChatSource telegram{.sourceName = "Telegram", .logFormat = "{{sourceName}} {{username}}: {{message}}"};
};

namespace {

Config config;

std::function<void(std::string)> sendTelegramMessage;


// Event listeners
ll::event::ListenerPtr playerJoinEventListener;
ll::event::ListenerPtr playerLeaveEventListener;


void broadcast(std::string_view message) {
    ll::service::getLevel()->getPacketSender()->sendBroadcast(TextPacket::createRawMessage(message));
}

void replaceString(std::string& subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

std::string
replacePlaceholder(const std::string& format, const ConfigChatSource& chatSource, const PlaceholderData& placeholders) {
    auto msg = std::string(format);
    replaceString(msg, "{{sourceName}}", chatSource.sourceName);
    replaceString(msg, "{{username}}", placeholders.username);
    replaceString(msg, "{{message}}", placeholders.message);
    return msg;
}

std::string getUsername(const TgBot::User::Ptr& user) {
    if (!user->username.empty()) return user->username;
    std::string fullname;
    if (!user->firstName.empty()) fullname += user->firstName;
    if (!user->lastName.empty()) {
        if (!fullname.empty()) fullname += " ";
        fullname += user->lastName;
    }
    if (!fullname.empty()) return fullname;
    return std::to_string(user->id);
}

} // namespace


namespace telegram_bot {

TelegramBotMod& TelegramBotMod::getInstance() {
    static TelegramBotMod instance;
    return instance;
}


// Credit: https://github.com/LordBombardir/LLPowerRanks/blob/main/src/mod/hooks/Hooks.cpp
LL_AUTO_TYPE_INSTANCE_HOOK(
    DisplayGameMessageHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::_displayGameMessage,
    void,
    const Player& sender,
    ChatEvent&    chatEvent
) {
    auto& player = const_cast<Player&>(sender);

    if (sendTelegramMessage) {
        try {
            const PlaceholderData placeholders{.username = player.getRealName(), .message = chatEvent.mMessage.get()};

            if (config.minecraft.userEnabled) {
                auto message = (replacePlaceholder(config.minecraft.userFormat, config.minecraft, placeholders));
                sendTelegramMessage(message);
            }
            if (config.minecraft.logEnabled) {
                TelegramBotMod::getInstance().getSelf().getLogger().info(
                    replacePlaceholder(config.minecraft.logFormat, config.minecraft, placeholders)
                );
            }
        } catch (const std::exception& e) {
            TelegramBotMod::getInstance().getSelf().getLogger().error(
                "DisplayGameMessage hook error: " + std::string(e.what())
            );
        }
    }

    return origin(sender, chatEvent);
}

bool TelegramBotMod::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

bool TelegramBotMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    return true;
}


void TelegramBotMod::runBotThread() {
    try {
        TgBot::Bot bot(config.telegramBotToken);

        // Setup bot commands/events
        bot.getEvents().onCommand("start", [&bot, this](const TgBot::Message::Ptr& message) {
            getSelf().getLogger().info(getUsername(message->from) + " used /start");
            bot.getApi().sendMessage(message->chat->id, "Minecraft Bot is running!");
        });

        bot.getEvents().onCommand("list", [&bot, this](const TgBot::Message::Ptr& message) {
            getSelf().getLogger().info(getUsername(message->from) + " used /list");

            auto&       players = ll::service::getLevel()->getPlayerList();
            const auto  online  = players.size();
            std::string text    = "Online " + std::to_string(online);
            if (online != 0) text += ":";

            for (const auto& player : players) {
                text += "\n" + player.second.mName.get();
            }

            bot.getApi().sendMessage(message->chat->id, text);
        });

        bot.getEvents().onAnyMessage([this](const TgBot::Message::Ptr& message) {
            if (config.telegramIgnoreOtherBots && message->from->isBot) return;
            if (config.telegramIgnoreOtherChats && message->chat->id != config.telegramChatId) return;
            if (config.telegramIgnoreCommands && message->text.starts_with("/")) return;

            const PlaceholderData placeholders{.username = getUsername(message->from), .message = message->text};

            if (config.telegram.userEnabled) {
                broadcast(replacePlaceholder(config.telegram.userFormat, config.telegram, placeholders));
            }
            if (config.telegram.logEnabled) {
                getSelf().getLogger().info(replacePlaceholder(config.telegram.logFormat, config.telegram, placeholders)
                );
            }
        });

        sendTelegramMessage = [&bot](const std::string& text) {
            bot.getApi().sendMessage(config.telegramChatId, text);
        };

        // Use long polling with timeout (5 seconds)
        TgBot::TgLongPoll longPoll(bot, 100, config.telegramPollingTimeoutSec);
        getSelf().getLogger().info("Bot long polling thread started");

        while (mBotRunning) {
            try {
                longPoll.start();
            } catch (const std::exception& e) {
                getSelf().getLogger().error("Polling error: " + std::string(e.what()));
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    } catch (const std::exception& e) {
        getSelf().getLogger().error("Bot fatal: " + std::string(e.what()));
    }
    getSelf().getLogger().info("Thread stopped");
}

bool TelegramBotMod::enable() {
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        getSelf().getLogger().warn("Cannot load configurations from {}", configFilePath);
        getSelf().getLogger().info("Saving default configurations");

        if (!ll::config::saveConfig(config, configFilePath)) {
            getSelf().getLogger().error("Cannot save default configurations to {}", configFilePath);
        }
    }

    std::string errorReason;


    if (config.telegramBotToken == "INSERT YOUR TOKEN HERE" || config.telegramBotToken.empty()) {
        errorReason = "No token provided in config.";
    }

    if (config.telegramChatId == 0) {
        if (!errorReason.empty()) errorReason += " ";
        errorReason += "No group id provided in config.";
    }

    if (!errorReason.empty()) {
        getSelf().getLogger().warn(
            errorReason + " Not starting bot. Edit " + configFilePath.string() + " and then run ll reload TelegramBot"
        );
        return false;
    }

    auto& eventBus = ll::event::EventBus::getInstance();


    playerJoinEventListener =
        eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>([](ll::event::player::PlayerJoinEvent& event) {
            const PlaceholderData placeholders{.username = event.self().getRealName(), .message = ""};

            auto message = replacePlaceholder(config.joinTextFormat, config.minecraft, placeholders);
            sendTelegramMessage(message);
        });

    playerLeaveEventListener =
        eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::player::PlayerDisconnectEvent& event) {
            const PlaceholderData placeholders{.username = event.self().getRealName(), .message = ""};

            auto message = replacePlaceholder(config.leaveTextFormat, config.minecraft, placeholders);
            sendTelegramMessage(message);
        });


    if (!mBotRunning) {
        mBotRunning = true;
        mBotThread  = std::thread(&TelegramBotMod::runBotThread, this);
        getSelf().getLogger().info("Enabled");
    }
    return true;
}

bool TelegramBotMod::disable() {
    if (mBotRunning) {
        mBotRunning = false;
        if (mBotThread.joinable()) mBotThread.join();
        sendTelegramMessage = [this](const std::string&) {
            getSelf().getLogger().warn("Call to disabled onMinecraftChat function. Enable mod to fix this");
        };
        getSelf().getLogger().info("Disabled");
    }

    auto& eventBus = ll::event::EventBus::getInstance();

    if (playerJoinEventListener && eventBus.hasListener(playerJoinEventListener)) {
        eventBus.removeListener(playerJoinEventListener);
    }

    if (playerLeaveEventListener && eventBus.hasListener(playerLeaveEventListener)) {
        eventBus.removeListener(playerLeaveEventListener);
    }

    return true;
}

} // namespace telegram_bot

LL_REGISTER_MOD(telegram_bot::TelegramBotMod, telegram_bot::TelegramBotMod::getInstance());
