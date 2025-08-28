#include "McCommand.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "TelegramBot/telegram/BotThread.h"
#include "TelegramBot/telegram/TgUtils.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOutputType.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandVersion.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include "mc/server/commands/MinecraftCommands.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/world/Minecraft.h"
#include "mc/world/level/dimension/Dimension.h"
#include <TelegramBot/telegram/TgUtils.h>
#include <ll/api/Config.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/service/Bedrock.h>
#include <mc/network/PacketSender.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/world/level/Level.h>
#include <tgbot/tgbot.h>


namespace telegram_bot::tgcommands {

struct CommandResult {
    std::string output;
    bool        success;
};

CommandResult runCommand(std::string& commandName, std::string& langCode) {
    try {
        std::string outputStr;
        auto        origin =
            ServerCommandOrigin("TelegramBot", ll::service::getLevel()->asServer(), CommandPermissionLevel::Owner, 0);

        auto command = ll::service::getMinecraft()->mCommands->compileCommand(
            ::HashedString(commandName),
            origin,
            (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [&](std::string const& err) { outputStr.append(err).append("\n"); }
        );
        bool success = false;

        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
            if (langCode.empty()) langCode = getI18n().getCurrentLanguage()->mCode;
            std::shared_ptr<Localization> localization = getI18n().getLocaleFor(langCode);

            for (auto& msg : output.mMessages) {
                outputStr += getI18n().get(msg.mMessageId, msg.mParams, localization).append("\n");
            }
            success = output.mSuccessCount ? true : false;
        }
        if (outputStr.ends_with('\n')) {
            outputStr.pop_back();
        }
        return {.output = outputStr, .success = success};
    } catch (const std::exception& e) {
        auto err = std::string(e.what());
        TelegramBotMod::getInstance().getSelf().getLogger().error("runCommand error: " + err);
        return {.output = err, .success = false};
    }
};

void mcCommand(TgBot::Bot& bot) {
    for (auto& cmd : config.customCommands.commands) {
        add(
            {.name        = cmd.name,
             .description = cmd.description,
             .adminOnly   = cmd.adminOnly,
             .listener =
                 [&bot, &cmd](const TgBot::Message::Ptr& message) {
                     std::string command = std::string(cmd.command);

                     bool hasParams = command.find("$1") != std::string::npos;
                     if (hasParams) {
                         auto params = getParams(message->text);
                         if (params.empty()) return telegram_bot::reply(bot, message, "Command requires params");
                         Utils::replaceString(command, "$1", params);
                     }
                     telegram_bot::reply(bot, message, "Executing command `" + command + "`...", "MarkdownV2");

                     auto         chatId  = message->chat->id;
                     std::int32_t topicId = message->isTopicMessage ? message->messageThreadId : 0;
                     ll::coro::keepThis([&command, chatId, topicId]() -> ll::coro::CoroTask<void> {
                         try {
                             auto result  = runCommand(command, config.customCommands.langCode);
                             auto message = Utils::escapeStringForTelegram(result.output);
                             sendTelegramMessage(message, chatId, topicId);
                         } catch (const std::exception& e) {
                             TelegramBotMod::getInstance().getSelf().getLogger().error(
                                 "telegram runCommand error: " + std::string(e.what())
                             );
                         }
                         co_return;
                     }).launch(ll::thread::ServerThreadExecutor::getDefault());
                 }}
        );
    }
};

} // namespace telegram_bot::tgcommands