#include "./Hooks.h"
#include "TelegramBot/Utils.h"
#include <TelegramBot/TelegramBot.h>
#include <TelegramBot/telegram/BotThread.h>
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


namespace telegram_bot::hooks {


// Credit: https://github.com/LordBombardir/LLPowerRanks/blob/main/src/mod/hooks/Hooks.cpp
LL_TYPE_INSTANCE_HOOK(
    DisplayGameMessageHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::_displayGameMessage,
    void,
    const Player& sender,
    ChatEvent&    chatEvent
) {
    if ((config.minecraftGlobalChatPrefix.empty() || chatEvent.mMessage->starts_with(config.minecraftGlobalChatPrefix)
        )) {
        auto& player = const_cast<Player&>(sender);

        try {
            const PlaceholderData placeholders{.username = player.getRealName(), .message = chatEvent.mMessage.get()};

            if (!config.telegram.chatFormat.empty()) {
                auto message = (Utils::replacePlaceholders(config.telegram.chatFormat, config.minecraft, placeholders));
                telegram_bot::sendTelegramMessage(message, config.telegramChatId);
            }
            if (!config.minecraft.consoleLogFormat.empty()) {
                TelegramBotMod::getInstance().getSelf().getLogger().info(
                    Utils::replacePlaceholders(config.minecraft.consoleLogFormat, config.minecraft, placeholders)
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

void enable() { DisplayGameMessageHook::hook(); };

void disable() { DisplayGameMessageHook::unhook(); };

} // namespace telegram_bot::hooks