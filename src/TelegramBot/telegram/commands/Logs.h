#pragma once
#include <tgbot/tgbot.h>

namespace telegram_bot::tgcommands {
void logs(TgBot::Bot& bot);

void cleanLogsStorage();
} // namespace telegram_bot::tgcommands