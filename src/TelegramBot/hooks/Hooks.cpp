#include "./Hooks.h"
#include "TelegramBot/Utils.h"
#include "mc/network/NetEventCallback.h"
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

// Credit: https://github.com/LordBombardir
// Designed to work with LLPowerRanks and ChatRadius
LL_TYPE_INSTANCE_HOOK(
    PlayerSendMessageHook,
    HookPriority::High,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    const NetworkIdentifier& identifier,
    const TextPacket&        packet
) {
    try {
        ServerPlayer* player =
            ll::service::getServerNetworkHandler()->_getServerPlayer(identifier, packet.mSenderSubId);

        if (player) {
            auto mMessage = packet.mMessage;
            if ((config.minecraftGlobalChatPrefix.empty() || mMessage.starts_with(config.minecraftGlobalChatPrefix))) {
                if (!config.minecraftGlobalChatPrefix.empty()) {
                    mMessage = mMessage.erase(0, config.minecraftGlobalChatPrefix.size());
                }

                const PlaceholderData placeholders{
                    .username = player->getRealName(),
                    .name     = player->getNameTag(),
                    .message  = mMessage,
                };

                if (!config.telegram.chatFormat.empty()) {
                    auto message =
                        (Utils::replacePlaceholders(config.telegram.chatFormat, config.minecraft, placeholders, true));
                    telegram_bot::queneTgMessage(message, config.telegramChatId);
                }
                if (!config.minecraft.consoleLogFormat.empty()) {
                    logger.info(
                        Utils::replacePlaceholders(config.minecraft.consoleLogFormat, config.minecraft, placeholders)
                    );
                }
            }
        }

        return origin(identifier, packet);
    } catch (const std::exception& e) {
        logger.error("PlayerSendMessageHook error: " + std::string(e.what()));
    }
}

void enable() { PlayerSendMessageHook::hook(); };

void disable() { PlayerSendMessageHook::unhook(); };

} // namespace telegram_bot::hooks