#pragma once

#include <string>

namespace telegram_bot {
void startThread();

void stopThread();

void sendTelegramMessage(const std::string& message, std::int64_t chatId = 0, std::int32_t topicId = 0);


} // namespace telegram_bot