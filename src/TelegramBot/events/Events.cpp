#include "Events.h"
#include "TelegramBot/Utils.h"
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
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerListEntry.h>
#include <mc/world/events/ChatEvent.h>
#include <mc/world/level/Level.h>


namespace telegram_bot::events {

ll::event::ListenerPtr playerJoinEventListener;
ll::event::ListenerPtr playerLeaveEventListener;
ll::event::ListenerPtr mobDieEventListener;

void subscribe() {

    auto& eventBus = ll::event::EventBus::getInstance();

    playerJoinEventListener =
        eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>([](ll::event::player::PlayerJoinEvent& event) {
            if (config.minecraft.joinTextFormat.empty()) return;

            const PlaceholderData placeholders{.username = event.self().getRealName(), .message = ""};

            auto message = Utils::replacePlaceholders(config.minecraft.joinTextFormat, config.minecraft, placeholders);
            sendTelegramMessage(message, config.telegramChatId);
        });

    playerLeaveEventListener =
        eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::player::PlayerDisconnectEvent& event) {
            if (config.minecraft.leaveTextFormat.empty()) return;

            const PlaceholderData placeholders{.username = event.self().getRealName(), .message = ""};

            auto message = Utils::replacePlaceholders(config.minecraft.leaveTextFormat, config.minecraft, placeholders);
            sendTelegramMessage(message, config.telegramChatId);
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

            if (!config.minecraft.deathTextFormat.empty()) {
                auto message =
                    Utils::replaceKillPlaceholders(config.minecraft.deathTextFormat, config.minecraft, placeholders);

                sendTelegramMessage(message, config.telegramChatId);
            }

            if (config.minecraft.deathTextLogFormat.empty()) {
                auto message =
                    Utils::replaceKillPlaceholders(config.minecraft.deathTextLogFormat, config.minecraft, placeholders);

                TelegramBotMod::getInstance().getSelf().getLogger().info(message);
            }
        }
    });
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
};
} // namespace telegram_bot::events