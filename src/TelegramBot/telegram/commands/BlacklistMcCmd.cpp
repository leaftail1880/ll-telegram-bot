#include "BlacklistMcCmd.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/telegram/TgUtils.h"
#include "tgbot/types/MessageEntity.h"


namespace telegram_bot::tgcommands {
void blacklistmccmd(TgBot::Bot& bot) {
    add(
        {.name        = "blacklistmccmd",
         .description = "Black list minecraft command from logging",
         .adminOnly   = true,
         .listener    = [&bot](const TgBot::Message::Ptr& message) -> void {
             std::string params = getParams(message->text);
             std::string command;

             if (!params.empty()) {
                 command = params;
             } else {
                 if (!message->replyToMessage) {
                     telegram_bot::reply(bot, message, "Reply to message to blacklist it!");
                     return;
                 }
                 if (message->replyToMessage->entities.empty()) {
                     telegram_bot::reply(bot, message, "Wrong reply message type, expected message with command!");
                     return;
                 }

                 int start = 0;
                 int end   = 0;
                 for (auto& entity : message->replyToMessage->entities) {
                     if (entity->type == TgBot::MessageEntity::Type::Code) {
                         start = entity->offset;
                         end   = entity->offset + entity->length;
                     }
                 }

                 if (end == 0 || start == end) {
                     telegram_bot::reply(bot, message, "Failed to find command code block in message");
                     return;
                 }

                 command = message->replyToMessage->text.substr(start, end);
             }

             telegram_bot::config.commandLogs.blacklist.push_back(command);
             telegram_bot::TelegramBotMod::getInstance().saveConfig();

             telegram_bot::reply(bot, message, "Blacklisted command '" + command + "'");
         }}
    );
};
} // namespace telegram_bot::tgcommands