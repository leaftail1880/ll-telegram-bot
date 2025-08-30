#include "Events.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/Utils.h"
#include "ll/api/event/command/ExecuteCommandEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "mc/world/actor/ActorDamageByActorSource.h"
#include "mc/world/actor/ActorDamageByBlockSource.h"
#include "mc/world/level/block/Block.h"
#include <TelegramBot/TelegramBot.h>
#include <TelegramBot/telegram/BotThread.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/entity/MobDieEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <mc/locale/I18n.h>
#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>


namespace telegram_bot::events {

ll::event::ListenerPtr playerJoinEventListener;
ll::event::ListenerPtr playerLeaveEventListener;
ll::event::ListenerPtr mobDieEventListener;
ll::event::ListenerPtr commandEventListener;

void subscribe() {

    auto& eventBus = ll::event::EventBus::getInstance();

    playerJoinEventListener =
        eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>([](ll::event::player::PlayerJoinEvent& event) {
            if (config.telegram.joinTextFormat.empty()) return;

            const PlaceholderData placeholders{.username = event.self().getRealName(), .name = "", .message = ""};

            auto message =
                Utils::replacePlaceholders(config.telegram.joinTextFormat, config.minecraft, placeholders, true);
            queneTgMessage(message);
        });

    playerLeaveEventListener =
        eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::player::PlayerDisconnectEvent& event) {
            if (config.telegram.leaveTextFormat.empty()) return;

            const PlaceholderData placeholders{.username = event.self().getRealName(), .name = "", .message = ""};

            auto message =
                Utils::replacePlaceholders(config.telegram.leaveTextFormat, config.minecraft, placeholders, true);
            queneTgMessage(message);
        });

    mobDieEventListener = eventBus.emplaceListener<ll::event::MobDieEvent>([](ll::event::MobDieEvent& event) {
        if (config.minecraft.deathTextFormat.empty()) return;


        KillPlaceholderData placeholders{
            .cause = ActorDamageSource::lookupCauseName(event.source().mCause),
        };

        if (event.self().isPlayer()) {
            auto& player                 = static_cast<Player&>(event.self());
            placeholders.deadMobOrPlayer = player.getRealName();
        } else {
            placeholders.deadMobOrPlayer = event.self().getNameTag();
        }

        if (event.source().isEntitySource()) {
            auto& source        = static_cast<const ActorDamageByActorSource&>(event.source());
            placeholders.killer = source.mEntityNameTag;
        } else if (event.source().isBlockSource()) {
            auto& source        = static_cast<const ActorDamageByBlockSource&>(event.source());
            placeholders.killer = source.mBlock->getTypeName();
        }


        if (!placeholders.deadMobOrPlayer.empty()) {
            auto [token, params]    = event.source().getDeathMessage(placeholders.deadMobOrPlayer, &event.self());
            placeholders.translated = (getI18n().get(token, params, getI18n().getLocaleFor(config.telegram.langCode)));

            if (!config.telegram.deathTextFormat.empty()) {
                auto message = Utils::replaceKillPlaceholders(
                    config.telegram.deathTextFormat,
                    config.minecraft,
                    placeholders,
                    true
                );

                queneTgMessage(message);
            }

            if (config.minecraft.deathTextLogFormat.empty()) {
                auto message =
                    Utils::replaceKillPlaceholders(config.minecraft.deathTextLogFormat, config.minecraft, placeholders);

                logger.info(message);
            }

            if (!config.minecraft.deathTextFormat.empty()) {
                auto message =
                    Utils::replaceKillPlaceholders(config.minecraft.deathTextFormat, config.minecraft, placeholders);

                Utils::broadcast(message);
            }
        }
    });

    commandEventListener = eventBus.emplaceListener<ll::event::command::ExecutedCommandEvent>(
        [](ll::event::command::ExecutedCommandEvent& event) {
            try {
                if (!config.commandLogs.enabled) return;

                auto        command = event.commandContext().mCommand;
                const auto& sender  = event.commandContext().mOrigin->getName();

                if (sender == "TelegramBot") return;

                if (command.starts_with("/")) command.erase(0, 1);

                bool wasEmergent = false;

                for (auto& emergency : config.commandLogs.emergency) {

                    if (command.starts_with(emergency.commandStartsWith)) {
                        bool ignored = false;
                        for (auto& cmd : emergency.blacklist) {
                            if (command.starts_with(cmd)) ignored = true;
                        };

                        if (ignored) continue;

                        wasEmergent = true;
                        logger.info("EMERGENT {} /{}", sender, command);
                        queneTgMessage(
                            Utils::replacePlaceholders(
                                emergency.message,
                                {.sourceName = "CommandLogEmergency"},
                                {
                                    .username = sender,
                                    .name     = sender,
                                    .message  = command,
                                },
                                true
                            ),
                            config.commandLogs.telegramChatId,
                            config.commandLogs.telegramTopicId
                        );
                    }
                }

                if (wasEmergent) return; // no need for duplicated messages

                bool shouldLog = true;

                if (config.commandLogs.whiteListEnabled) {
                    for (auto& whitelistCmd : config.commandLogs.whitelist) {
                        if (command.starts_with(whitelistCmd)) {
                            shouldLog = true;
                            break;
                        }
                    }

                } else {
                    for (auto& blacklistCmd : config.commandLogs.blacklist) {
                        if (command.starts_with(blacklistCmd)) {
                            shouldLog = false;
                            break;
                        }
                    }
                }

                if (shouldLog) {
                    logger.info("{} /{}", sender, command);

                    queneTgMessage(
                        Utils::replacePlaceholders(
                            config.commandLogs.message,
                            {.sourceName = "CommandLog"},
                            {
                                .username = sender,
                                .name     = sender,
                                .message  = command,
                            },
                            true
                        ),
                        config.commandLogs.telegramChatId,
                        config.commandLogs.telegramTopicId
                    );
                }
            } catch (const std::exception& e) {
                logger.error("Command event listener error: " + std::string(e.what()));
            }
        }
    );
};

void unsubscribe() {
    auto& eventBus = ll::event::EventBus::getInstance();

    if (playerJoinEventListener && eventBus.hasListener(playerJoinEventListener)) {
        eventBus.removeListener(playerJoinEventListener);
    }

    if (playerLeaveEventListener && eventBus.hasListener(playerLeaveEventListener)) {
        eventBus.removeListener(playerLeaveEventListener);
    }

    if (mobDieEventListener && eventBus.hasListener(mobDieEventListener)) {
        eventBus.removeListener(mobDieEventListener);
    }

    if (commandEventListener && eventBus.hasListener(commandEventListener)) {
        eventBus.removeListener(commandEventListener);
    }
};
} // namespace telegram_bot::events