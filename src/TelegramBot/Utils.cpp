#include "TelegramBot/Utils.h"
#include <ll/api/service/Bedrock.h>
#include <mc/network/PacketSender.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/world/level/Level.h>
#include <regex>

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


std::string Utils::escapeStringForTelegram(const std::string& input) {
    std::regex pattern(R"(([_\*\[\]\(\)~`>#\+-=\|{}\.!\\]))");
    return std::regex_replace(input, pattern, "\\$1");
}


std::string Utils::replacePlaceholders(
    const std::string&      format,
    const ConfigChatSource& chatSource,
    const PlaceholderData&  placeholders,
    bool                    escapeForTelegram
) {
    auto msg = std::string(format);
    replaceString(msg, "{{sourceName}}", chatSource.sourceName);
    replaceString(
        msg,
        "{{username}}",
        escapeForTelegram ? escapeStringForTelegram(placeholders.username) : placeholders.username
    );
    replaceString(
        msg,
        "{{message}}",
        escapeForTelegram ? escapeStringForTelegram(placeholders.message) : placeholders.message
    );
    replaceString(msg, "{{name}}", escapeForTelegram ? escapeStringForTelegram(placeholders.name) : placeholders.name);
    return msg;
}

std::string Utils::replaceKillPlaceholders(
    const std::string&         format,
    const ConfigChatSource&    chatSource,
    const KillPlaceholderData& placeholders,
    bool                       escapeForTelegram
) {
    auto msg = std::string(format);
    replaceString(
        msg,
        "{{sourceName}}",
        escapeForTelegram ? escapeStringForTelegram(chatSource.sourceName) : chatSource.sourceName
    );
    replaceString(
        msg,
        "{{deadMobOrPlayer}}",
        escapeForTelegram ? escapeStringForTelegram(placeholders.deadMobOrPlayer) : placeholders.deadMobOrPlayer
    );
    replaceString(
        msg,
        "{{killer}}",
        escapeForTelegram ? escapeStringForTelegram(placeholders.killer) : placeholders.killer
    );
    replaceString(msg, "{{cause}}", placeholders.cause);
    replaceString(
        msg,
        "{{translated}}",
        escapeForTelegram ? escapeStringForTelegram(placeholders.translated) : placeholders.translated
    );
    return msg;
}

} // namespace telegram_bot