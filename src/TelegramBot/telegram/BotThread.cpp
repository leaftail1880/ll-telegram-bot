#include "./BotThread.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include <ll/api/Config.h>
#include <ll/api/service/Bedrock.h>
#include <mc/server/ServerPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>
#include <tgbot/tgbot.h>
#include <thread>


namespace telegram_bot {

[[nodiscard]] ll::mod::NativeMod& getSelf() { return TelegramBotMod::getInstance().getSelf(); };

std::string getUsername(const TgBot::User::Ptr& user) {
    if (!user->username.empty()) return user->username;
    std::string fullname;
    if (!user->firstName.empty()) fullname += user->firstName;
    if (!user->lastName.empty()) {
        if (!fullname.empty()) fullname += " ";
        fullname += user->lastName;
    }
    if (!fullname.empty()) return fullname;
    return std::to_string(user->id);
}


struct OutgoingTelegramMessage {
    std::string  text;
    std::int64_t chatId;
};

struct OutgoingMinecraftMessage {
    std::string text;
};

std::mutex                          outgoingMsgTelegramMutex;
std::queue<OutgoingTelegramMessage> outgoingMsgTelegramQueue;


std::mutex                           outgoingMsgMinecraftMutex;
std::queue<OutgoingMinecraftMessage> outgoingMsgMinecraftQueue;

void threadSafeBroadcast(const std::string& message) {
    std::lock_guard lock(outgoingMsgMinecraftMutex);
    outgoingMsgMinecraftQueue.push({.text = message});
}

void sendTelegramMessage(const std::string& message, std::int64_t chat) {
    std::lock_guard lock(outgoingMsgTelegramMutex);
    outgoingMsgTelegramQueue.push({.text = message, .chatId = chat});
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

            auto chatId = message->chat->id;
            ll::coro::keepThis([chatId]() -> ll::coro::CoroTask<> {
                auto&       players = ll::service::getLevel()->getPlayerList();
                const auto  online  = players.size();
                std::string text    = "Online " + std::to_string(online);
                if (online != 0) text += ":";

                for (const auto& player : players) {
                    text += "\n" + player.second.mName.get();
                }

                sendTelegramMessage(text, chatId);
                co_return;
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
        });

        bot.getEvents().onAnyMessage([](const TgBot::Message::Ptr& message) {
            try {
                if (!message) return;
                if (config.telegramIgnoreOtherBots && message->from->isBot) return;
                if (config.telegramIgnoreOtherChats && message->chat->id != config.telegramChatId) return;
                if (config.telegramIgnoreCommands && message->text.starts_with("/")) return;
                if (config.telegramTopicId != 1 && config.telegramTopicId != message->messageThreadId) return;

                const PlaceholderData placeholders{.username = getUsername(message->from), .message = message->text};

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
                longPoll.start();
            } catch (const std::exception& e) {
                getSelf().getLogger().error("Polling error: " + std::string(e.what()));
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            // Process queued messages
            std::queue<OutgoingTelegramMessage> batch;
            {
                std::lock_guard lock(outgoingMsgTelegramMutex);
                batch.swap(outgoingMsgTelegramQueue);
            }

            while (!batch.empty()) {
                try {
                    auto& message = batch.front();
                    bot.getApi().sendMessage(
                        message.chatId,
                        message.text,
                        nullptr,
                        nullptr,
                        nullptr,
                        "",
                        false,
                        std::vector<TgBot::MessageEntity::Ptr>(),
                        config.telegramTopicId == -1 ? 0 : config.telegramTopicId
                    );
                } catch (const std::exception& e) {
                    getSelf().getLogger().error("Telegram quened send message failed: " + std::string(e.what()));
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

using namespace ll::chrono_literals;

void startThread() {
    if (!mBotRunning) {
        mBotRunning = true;
        mBotThread  = std::thread(&runTelegramBot);

        ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
            while (mBotRunning) {
                co_await 10_tick;
                std::queue<OutgoingMinecraftMessage> batch;
                {
                    std::lock_guard lock(outgoingMsgMinecraftMutex);
                    batch.swap(outgoingMsgMinecraftQueue);
                }

                while (!batch.empty()) {
                    try {
                        auto& message = batch.front();
                        Utils::broadcast(message.text);
                    } catch (const std::exception& e) {
                        getSelf().getLogger().error("Minecraft send message failed: " + std::string(e.what()));
                    }
                    batch.pop();
                }
            }
            co_return;
        }).launch(ll::thread::ServerThreadExecutor::getDefault());
    }
}
} // namespace telegram_bot
