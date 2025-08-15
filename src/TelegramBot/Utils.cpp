#include "TelegramBot/Utils.h"
#include <ll/api/service/Bedrock.h>
#include <mc/network/PacketSender.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/world/level/Level.h>


namespace telegram_bot {


void Utils::broadcast(std::string_view message) {
    ll::service::getLevel()->getPacketSender()->sendBroadcast(TextPacket::createRawMessage(message));
}

void Utils::replaceString(std::string& subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

std::string Utils::replacePlaceholders(
    const std::string&      format,
    const ConfigChatSource& chatSource,
    const PlaceholderData&  placeholders
) {
    auto msg = std::string(format);
    replaceString(msg, "{{sourceName}}", chatSource.sourceName);
    replaceString(msg, "{{username}}", placeholders.username);
    replaceString(msg, "{{message}}", placeholders.message);
    replaceString(msg, "{{name}}", placeholders.name);
    return msg;
}

std::string Utils::replaceKillPlaceholders(
    const std::string&         format,
    const ConfigChatSource&    chatSource,
    const KillPlaceholderData& placeholders
) {
    auto msg = std::string(format);
    replaceString(msg, "{{sourceName}}", chatSource.sourceName);
    replaceString(msg, "{{deadMobOrPlayer}}", placeholders.deadMobOrPlayer);
    replaceString(msg, "{{killer}}", placeholders.killer);
    replaceString(msg, "{{cause}}", placeholders.cause);
    replaceString(msg, "{{translated}}", placeholders.translated);
    return msg;
}

} // namespace telegram_bot