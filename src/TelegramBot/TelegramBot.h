#pragma once

#include "ll/api/mod/NativeMod.h"

namespace telegram_bot {

extern std::function<void(std::string)> sendTelegramMessage;

class TelegramBotMod {

public:
    static TelegramBotMod& getInstance();

    TelegramBotMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    /// @return True if the mod is unloaded successfully.
    bool unload();


private:
    ll::mod::NativeMod& mSelf;
};

} // namespace telegram_bot
