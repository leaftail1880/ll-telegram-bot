#include "TgUtils.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "tgbot/types/BotCommand.h"
#include "tgbot/types/BotCommandScopeChat.h"
#include "tgbot/types/Message.h"
#include <memory>

namespace telegram_bot::tgcommands {

std::vector<Command> mCommands;

bool exists(const std::string& command) {
    return std::ranges::any_of(mCommands, [&command](auto& cmd) { return cmd.name == command; });
}

void add(const Command& command) {
    if (exists(command.name)) {
        logger.error("Cmd already exists " + command.name);
    } else {
        mCommands.push_back(command);
    }
};

void disable() { mCommands.clear(); };

bool isAllowed(const Command& cmd, const TgBot::Message::Ptr& message) {
    if (!cmd.adminOnly) return true;
    if (message->chat->id != telegram_bot::config.telegramAdminChatId) return false;
    return true;
}


std::vector<TgBot::BotCommand::Ptr> botCmdFromCmd(const std::vector<Command>& cmds) {
    std::vector<TgBot::BotCommand::Ptr> botCmds;

    for (auto& command : cmds) {
        TgBot::BotCommand::Ptr cmd = std::make_shared<TgBot::BotCommand>();
        cmd->command               = command.name;
        cmd->description           = command.description;
        botCmds.push_back(cmd);
    }

    return botCmds;
}

void subscribe(TgBot::Bot& bot) {
    std::vector<Command> userCommands;
    std::vector<Command> adminCommands;

    for (auto& cmd : mCommands) {
        if (!cmd.adminOnly) {
            userCommands.push_back(cmd);
        }

        adminCommands.push_back(cmd);

        bot.getEvents().onCommand(cmd.name, [&bot, &cmd](const TgBot::Message::Ptr& message) {
            logger.info(telegram_bot::getUsername(message->from) + " used " + message->text);

            if (!isAllowed(cmd, message)) {
                reply(bot, message, "This command is not allowed here!");
                return;
            }

            try {
                cmd.listener(message);
            } catch (const std::exception& e) {
                logger.error("Error in bot command " + cmd.name + ": " + std::string(e.what()));
            }
        });
    }

    bot.getApi().setMyCommands(botCmdFromCmd(userCommands));

    if (config.telegramAdminChatId != 0) {
        auto placeholder    = std::make_shared<TgBot::BotCommandScopeChat>();
        placeholder->chatId = config.telegramAdminChatId;
        bot.getApi().setMyCommands(botCmdFromCmd(adminCommands), placeholder);
    }
};


} // namespace telegram_bot::tgcommands


namespace telegram_bot {
std::string getName(const TgBot::User::Ptr& user) {
    std::string fullname;
    if (!user->firstName.empty()) fullname += user->firstName;
    if (!user->lastName.empty()) {
        if (!fullname.empty()) fullname += " ";
        fullname += user->lastName;
    }
    if (!fullname.empty()) return fullname;
    if (!user->username.empty()) return user->username;
    return std::to_string(user->id);
}

std::string getUsername(const TgBot::User::Ptr& user) {
    if (!user->username.empty()) return user->username;
    return getName(user);
}

TgBot::Message::Ptr
reply(TgBot::Bot& bot, const TgBot::Message::Ptr& message, const std::string& text, const std::string& parseMode) {
    return bot.getApi().sendMessage(
        message->chat->id,
        text,
        nullptr,
        nullptr,
        nullptr,
        parseMode,
        false,
        {},
        message->isTopicMessage ? message->messageThreadId : 0
    );
}

std::string getParams(const std::string& text) {
    auto space = text.find(' ');
    if (space == std::string::npos) {
        return ""; // or return text if you want the command itself when no params
    }
    return text.substr(space + 1); // +1 to skip the space
};
} // namespace telegram_bot