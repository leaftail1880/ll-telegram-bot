#include "Start.h"
#include "TelegramBot/telegram/TgUtils.h"


namespace telegram_bot::tgcommands {
void start(TgBot::Bot& bot) {
    add(
        {.name        = "start",
         .description = "Check if bot is running",
         .adminOnly   = false,
         .listener    = [&bot](const TgBot::Message::Ptr& message
                     ) { telegram_bot::reply(bot, message, "Minecraft Bot is running!"); }}
    );
};
} // namespace telegram_bot::tgcommands