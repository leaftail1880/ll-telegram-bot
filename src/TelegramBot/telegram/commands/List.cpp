#include "List.h"
#include "TelegramBot/Utils.h"
#include "TelegramBot/telegram/BotThread.h"
#include "TelegramBot/telegram/TgUtils.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include <ll/api/service/Bedrock.h>
#include <mc/server/ServerPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>


namespace telegram_bot::tgcommands {
void list() {
    add(
        {.name        = "list",
         .description = "List online players",
         .adminOnly   = false,
         .listener =
             [](const TgBot::Message::Ptr& message) {
                 auto chatId  = message->chat->id;
                 auto topicId = message->isTopicMessage ? message->messageThreadId : 0;

                 ll::coro::keepThis([chatId, topicId]() -> ll::coro::CoroTask<> {
                     auto&       players = ll::service::getLevel()->getPlayerList();
                     const auto  online  = players.size();
                     std::string text    = "**Online " + std::to_string(online) + "**";
                     if (online != 0) text += ":";

                     for (const auto& player : players) {
                         text += "\n" + Utils::escapeStringForTelegram(player.second.mName.get());
                     }

                     sendTelegramMessage(text, chatId, topicId);

                     co_return;
                 }).launch(ll::thread::ServerThreadExecutor::getDefault());
             }}
    );
};
} // namespace telegram_bot::tgcommands