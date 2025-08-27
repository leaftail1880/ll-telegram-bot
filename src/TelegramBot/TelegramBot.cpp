#include "TelegramBot.h"
#include "TelegramBot/events/Events.h"
#include "TelegramBot/hooks/Hooks.h"
#include "TelegramBot/telegram/BotThread.h"
#include "TelegramBot/telegram/TgUtils.h"
#include "ll/api/mod/RegisterHelper.h"
#include <TelegramBot/Utils.h>
#include <ll/api/Config.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/entity/MobDieEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/Command.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>

namespace telegram_bot {

Config config;

TelegramBotMod& TelegramBotMod::getInstance() {
    static TelegramBotMod instance;
    return instance;
}

struct BroadcastMessageCmdParams {
    std::string message{};
};

bool TelegramBotMod::load() {


    telegram_bot::hooks::enable();

    return true;
}

bool TelegramBotMod::unload() {
    telegram_bot::hooks::disable();
    return true;
}

std::filesystem::path getConfigPath() { return TelegramBotMod::getInstance().getSelf().getConfigDir() / "config.json"; }

bool TelegramBotMod::saveConfig() {
    const auto& configFilePath = getConfigPath();
    if (!ll::config::saveConfig(config, configFilePath)) {
        getSelf().getLogger().error("Cannot save default configurations to {}", configFilePath);
        return false;
    }
    return true;
}

bool TelegramBotMod::enable() {
    const auto& configFilePath = getConfigPath();
    if (!ll::config::loadConfig(config, configFilePath)) {
        getSelf().getLogger().warn("Cannot load configurations from {}", configFilePath);
        getSelf().getLogger().info("Saving default configurations");

        if (!saveConfig()) return false;
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

    auto commandRegistry = ll::service::getCommandRegistry();
    if (!commandRegistry) {
        throw std::runtime_error("failed to get command registry");
    }

    auto& command = ll::command::CommandRegistrar::getInstance().getOrCreateCommand(
        "broadcastchatmessage",
        "Sends message to other chats (telegram).",
        CommandPermissionLevel::GameDirectors
    );

    command.overload<BroadcastMessageCmdParams>().required("message").execute(
        [](CommandOrigin const&, CommandOutput& output, BroadcastMessageCmdParams const& param) {
            output.success("Message '" + param.message + "' sent");
            sendTelegramMessage(Utils::escapeStringForTelegram(param.message));
        }
    );


    telegram_bot::startThread();
    telegram_bot::events::subscribe();

    return true;
}

bool TelegramBotMod::disable() {
    telegram_bot::stopThread();
    telegram_bot::events::unsubscribe();
    telegram_bot::tgcommands::disable();

    return true;
}

} // namespace telegram_bot

LL_REGISTER_MOD(telegram_bot::TelegramBotMod, telegram_bot::TelegramBotMod::getInstance());
