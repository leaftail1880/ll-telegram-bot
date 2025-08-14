#pragma once

#include <string>

namespace telegram_bot {
void startThread();

void stopThread();

void sendTelegramMessage(const std::string& message, std::int64_t chat);

} // namespace telegram_bot