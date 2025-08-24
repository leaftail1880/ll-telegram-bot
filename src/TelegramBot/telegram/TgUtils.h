#pragma once

#include <tgbot/tgbot.h>

namespace telegram_bot::tgcommands {

struct Command {
    std::string name;
    std::string description;
    bool        adminOnly;

    TgBot::EventBroadcaster::MessageListener listener;
};


void add(const Command& command);

void subscribe(TgBot::Bot& bot);

} // namespace telegram_bot::tgcommands

namespace telegram_bot {
std::string getName(const TgBot::User::Ptr& user);

std::string getUsername(const TgBot::User::Ptr& user);

void reply(
    TgBot::Bot&                bot,
    const TgBot::Message::Ptr& message,
    const std::string&         text,
    const std::string&         parseMode = ""
);
} // namespace telegram_bot