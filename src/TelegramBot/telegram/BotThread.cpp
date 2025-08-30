#include "./BotThread.h"
#include "./commands/BlacklistMcCmd.h"
#include "./commands/List.h"
#include "./commands/Logs.h"
#include "./commands/McCommand.h"
#include "./commands/Start.h"
#include "MyCurlHttpClient.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "commands/Logs.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include <TelegramBot/telegram/TgUtils.h>
#include <ll/api/Config.h>
#include <ll/api/chrono/GameChrono.h>
#include <memory>
#include <regex>
#include <tgbot/tgbot.h>
#include <thread>


namespace telegram_bot {
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
            logger.error("threadSafeBroadcast error: " + std::string(e.what()));
        }
        co_return;
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

std::string removeFormatCodes(const std::string& input) {
    std::regex pattern("§.");
    return std::regex_replace(input, pattern, "");
}

void queneTgMessage(const std::string& message, std::int64_t chatId, std::int32_t topicId) {
    std::lock_guard lock(outgoingMsgTelegramMutex);

    if (chatId == 0) chatId = config.telegramChatId;
    if (chatId == config.telegramChatId && topicId == 0) topicId = config.telegramTopicId;

    outgoingMsgTelegramQueue.push({
        config.telegram.clearFromColorCodes ? removeFormatCodes(message) : message,
        chatId,
        topicId,
    });
}

std::atomic<bool> mBotRunning = false;
std::thread       mBotThread;
std::thread       mMessagesThread;

void runTelegramBot() {
    try {
        TgBot::MyCurlHttpClient httpClient;
        TgBot::Bot              bot(telegram_bot::config.telegramBotToken, httpClient);

        // Setup bot commands/events
        telegram_bot::tgcommands::start(bot);
        telegram_bot::tgcommands::list();
        telegram_bot::tgcommands::blacklistmccmd(bot);
        telegram_bot::tgcommands::mcCommand(bot);
        telegram_bot::tgcommands::logs(bot);

        telegram_bot::tgcommands::subscribe(bot);

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
                    logger.info(
                        Utils::replacePlaceholders(config.telegram.consoleLogFormat, config.telegram, placeholders)
                    );
                }
            } catch (const std::exception& e) {
                logger.error("onAnyMessage error: " + std::string(e.what()));
            }
        });

        TgBot::TgLongPoll longPoll(bot); // timeout is unused here. Thanks to tgbotcpp devs
        logger.info(
            "Bot long polling thread started, username={} timeout={} chat={} topic={}",
            bot.getApi().getMe()->username,
            config.telegramTimeoutSec,
            config.telegramChatId,
            config.telegramTopicId == -1 ? "no topic" : std::to_string(config.telegramTopicId)
        );

        if (!config.telegramStartMessage.empty()) {
            bot.getApi().sendMessage(
                config.telegramChatId,
                config.telegramStartMessage,
                nullptr,
                nullptr,
                nullptr,
                "MarkdownV2",
                false,
                {},
                config.telegramTopicId == -1 ? 0 : config.telegramTopicId
            );
        }

        while (mBotRunning) {
            try {
                longPoll.start();
            } catch (const std::exception& e) {
                logger.error("Polling error: " + std::string(e.what()));
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

        if (!config.telegramStopMessage.empty()) {
            bot.getApi().sendMessage(
                config.telegramChatId,
                config.telegramStopMessage,
                nullptr,
                nullptr,
                nullptr,
                "MarkdownV2",
                false,
                {},
                config.telegramTopicId == -1 ? 0 : config.telegramTopicId
            );
        }
    } catch (const std::exception& e) {
        logger.error("Bot thread fatal: " + std::string(e.what()));
    }

    logger.info("Thread stopped");
}

void runMessagesThread() {
    try {

        logger.info("Message sender thread started");

        TgBot::MyCurlHttpClient httpClient;
        TgBot::Bot              bot(telegram_bot::config.telegramBotToken, httpClient);

        while (mBotRunning) {
            // Process queued messages
            std::queue<OutgoingTelegramMessage> batch;
            {
                std::lock_guard lock(outgoingMsgTelegramMutex);
                batch.swap(outgoingMsgTelegramQueue);
            }

            while (!batch.empty() && mBotRunning) {
                auto& message = batch.front();
                try {
                    bot.getApi().sendMessage(
                        message.chatId,
                        message.text,
                        nullptr,
                        nullptr,
                        nullptr,
                        "MarkdownV2",
                        false,
                        {},
                        message.topicId == -1 ? 0 : message.topicId
                    );
                } catch (const std::exception& e) {
                    logger.error(
                        "Telegram quened send message '" + message.text + "' failed: " + std::string(e.what())
                    );
                }
                batch.pop();
            }
        }
    } catch (const std::exception& e) {
        logger.error("Bot sender thread fatal: " + std::string(e.what()));
    }

    logger.info("Thread sender thread stopped");
}


void stopThread() {
    if (mBotRunning) {
        logger.info("Stopping threads...");
        mBotRunning = false;

        if (mMessagesThread.joinable()) mMessagesThread.join();
        if (mBotThread.joinable()) mBotThread.join();
    }

    tgcommands::disable();
    tgcommands::cleanLogsStorage();
}

void startThread() {
    if (!mBotRunning) {
        mBotRunning     = true;
        mBotThread      = std::thread(&runTelegramBot);
        mMessagesThread = std::thread(&runMessagesThread);
    }
}

} // namespace telegram_bot
