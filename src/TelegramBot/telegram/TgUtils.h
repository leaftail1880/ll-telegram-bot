#pragma once

#include <tgbot/tgbot.h>

namespace telegram_bot::tgcommands {

struct Command {
    std::string name;
    std::string description;
    bool        adminOnly;

    TgBot::EventBroadcaster::MessageListener listener;
};

bool exists(const std::string& command);

void add(const Command& command);

void disable();

void subscribe(TgBot::Bot& bot);

} // namespace telegram_bot::tgcommands

namespace telegram_bot {
std::string getName(const TgBot::User::Ptr& user);

std::string getUsername(const TgBot::User::Ptr& user);

TgBot::Message::Ptr
reply(TgBot::Bot& bot, const TgBot::Message::Ptr& message, const std::string& text, const std::string& parseMode = "");

std::string getParams(const std::string& text);
} // namespace telegram_bot