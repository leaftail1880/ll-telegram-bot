#include "./BotThread.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "tgbot/types/LinkPreviewOptions.h"
#include <ll/api/Config.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/service/Bedrock.h>
#include <mc/server/ServerPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>
#include <regex>
#include <tgbot/tgbot.h>
#include <thread>


namespace telegram_bot {

[[nodiscard]] ll::mod::NativeMod& getSelf() { return TelegramBotMod::getInstance().getSelf(); };

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

struct OutgoingTelegramMessage {
    std::string  text;
    std::int64_t chatId;
    std::int32_t topicId;
};

std::mutex                          outgoingMsgTelegramMutex;
std::queue<OutgoingTelegramMessage> outgoingMsgTelegramQueue;


void threadSafeBroadcast(const std::string& message) {
    ll::coro::keepThis([message]() -> ll::coro::CoroTask<void> {
        try {
            Utils::broadcast(message);
        } catch (const std::exception& e) {
            getSelf().getLogger().error("threadSafeBroadcast error: " + std::string(e.what()));
        }
        co_return;
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

std::string removeFormatCodes(const std::string& input) {
    std::regex pattern("§.");
    return std::regex_replace(input, pattern, "");
}

void sendTelegramMessage(const std::string& message, std::int64_t chatId, std::int32_t topicId) {
    std::lock_guard lock(outgoingMsgTelegramMutex);

    if (chatId == 0) chatId = config.telegramChatId;
    if (topicId == 0) topicId = config.telegramTopicId;

    outgoingMsgTelegramQueue.push({
        config.telegram.clearFromColorCodes ? removeFormatCodes(message) : message,
        chatId,
        topicId,
    });
}

std::atomic<bool> mBotRunning = false;
std::thread       mBotThread;

void runTelegramBot() {
    try {
        TgBot::Bot bot(telegram_bot::config.telegramBotToken);

        // Setup bot commands/events
        bot.getEvents().onCommand("start", [&bot](const TgBot::Message::Ptr& message) {
            getSelf().getLogger().info(getUsername(message->from) + " used /start");
            bot.getApi().sendMessage(message->chat->id, "Minecraft Bot is running!");
        });

        bot.getEvents().onCommand("list", [](const TgBot::Message::Ptr& message) {
            getSelf().getLogger().info(getUsername(message->from) + " used /list");

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
        });


        bot.getEvents().onAnyMessage([](const TgBot::Message::Ptr& message) {
            try {
                if (!message) return;
                if (config.telegramIgnoreOtherBots && message->from->isBot) return;
                if (config.telegramIgnoreOtherChats && message->chat->id != config.telegramChatId) return;
                if (message->text.empty()) return;
                if (config.telegramIgnoreCommands && message->text.starts_with("/")) return;
                if (config.telegramTopicId != -1 && config.telegramTopicId != message->messageThreadId) return;

                const PlaceholderData placeholders{
                    .username = getUsername(message->from),
                    .name     = getName(message->from),
                    .message  = message->text
                };

                if (!config.minecraft.chatFormat.empty()) {
                    threadSafeBroadcast(
                        Utils::replacePlaceholders(config.minecraft.chatFormat, config.telegram, placeholders)
                    );
                }
                if (!config.telegram.consoleLogFormat.empty()) {
                    getSelf().getLogger().info(
                        Utils::replacePlaceholders(config.telegram.consoleLogFormat, config.telegram, placeholders)
                    );
                }
            } catch (const std::exception& e) {
                getSelf().getLogger().error("onAnyMessage error: " + std::string(e.what()));
            }
        });

        TgBot::TgLongPoll longPoll(bot, 100, config.telegramPollingTimeoutSec);
        getSelf().getLogger().info(
            "Bot long polling thread started, username={} timeout={} chat={} topic={}",
            bot.getApi().getMe()->username,
            config.telegramPollingTimeoutSec,
            config.telegramChatId,
            config.telegramTopicId == -1 ? "no topic" : std::to_string(config.telegramTopicId)
        );

        while (mBotRunning) {
            try {
                getSelf().getLogger().info(
                        "longPoll start")
                );
                longPoll.start();
            } catch (const std::exception& e) {
                getSelf().getLogger().error("Polling error: " + std::string(e.what()));
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            // Process queued messages
            std::queue<OutgoingTelegramMessage> batch;
            {
                getSelf().getLogger().info(
                    "swapping quenes {}", batch.size())
                );
                std::lock_guard lock(outgoingMsgTelegramMutex);
                batch.swap(outgoingMsgTelegramQueue);
            }

            while (!batch.empty() && mBotRunning) {
                auto& message = batch.front();
                try {
                    getSelf().getLogger().info(
                        "sendMessage {}", message.text)
                    );
                    bot.getApi().sendMessage(
                        message.chatId,
                        message.text,
                        nullptr,
                        nullptr,
                        nullptr,
                        "MarkdownV2",
                        false,
                        std::vector<TgBot::MessageEntity::Ptr>(),
                        message.topicId == -1 ? 0 : message.topicId
                    );
                } catch (const std::exception& e) {
                    getSelf().getLogger().error(
                        "Telegram quened send message '" + message.text + "' failed: " + std::string(e.what())
                    );
                }
                batch.pop();
            }
        }


    } catch (const std::exception& e) {
        getSelf().getLogger().error("Bot fatal: " + std::string(e.what()));
    }
    getSelf().getLogger().info("Thread stopped");
}


void stopThread() {
    if (mBotRunning) {
        getSelf().getLogger().info("Stopping thread...");
        mBotRunning = false;

        if (mBotThread.joinable()) mBotThread.join();
    }
}

void startThread() {
    if (!mBotRunning) {
        mBotRunning = true;
        mBotThread  = std::thread(&runTelegramBot);
    }
}

} // namespace telegram_bot
