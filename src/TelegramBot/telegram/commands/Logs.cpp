#include "Logs.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "TelegramBot/telegram/TgUtils.h"
#include "tgbot/types/InlineKeyboardButton.h"
#include "tgbot/types/InlineKeyboardMarkup.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <utf8.h>

#define TG_MESSAGE_LIMIT 4096
#define PAGE_LIMIT       (TG_MESSAGE_LIMIT - 20)

size_t utf8Size(const std::string& str) {
    std::string::const_iterator it_start = str.begin();
    std::string::const_iterator it_end   = str.end();

    return static_cast<size_t>(utf8::distance(it_start, it_end));
}

std::string utf8Substr(const std::string& str, size_t start, size_t length = std::string::npos) {
    if (str.empty()) return "";

    // Declare iterators
    std::string::const_iterator it_start = str.begin();
    std::string::const_iterator it_end   = str.end();


    // Advance to the start position
    utf8::advance(it_start, start, str.end());

    // If length is specified, find the end iterator
    if (length != std::string::npos && (start + length) < utf8Size(str)) {
        it_end = it_start;
        utf8::advance(it_end, length, str.end());
    }

    return {it_start, it_end};
}

std::string getUuid() {
    static std::random_device dev;
    static std::mt19937       rng(dev());

    std::uniform_int_distribution<int> dist(0, 15);

    const char* v = "0123456789abcdef";
    const bool  dash[] =
        {false, false, false, false, true, false, true, false, true, false, true, false, false, false, false, false};

    std::string res;
    for (bool i : dash) {
        if (i) res += "-";
        res += v[dist(rng)];
        res += v[dist(rng)];
    }
    return res;
}

// using namespace csv;

std::chrono::year_month_day getNow() {
    auto now = std::chrono::system_clock::now();
    return {std::chrono::floor<std::chrono::days>(now)};
}

struct LogSearchParams {
    std::string name;
    std::string text;
    int         year  = static_cast<int>(getNow().year());
    unsigned    month = static_cast<unsigned>(getNow().month());
    unsigned    day   = static_cast<unsigned>(getNow().day());
};


int parseAndValidate(const std::string& argName, const std::string& s, int min, int max) {
    int val = 0;
    try {
        val = std::stoi(s);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(argName + ": Expected number, got: " + s);
    }

    if (val < min || val > max) {
        throw std::out_of_range(
            argName + ": Value " + s + " is out of range [" + std::to_string(min) + ", " + std::to_string(max)
        );
    }
    return val;
};

LogSearchParams parseLogSearchParams(const std::string& input) {
    LogSearchParams params;

    // Regular expression to match key=value pairs, with values optionally quoted
    std::regex           pattern(R"((\w+)=(\"[^\"]*\"|[^\s\"]+))");
    std::sregex_iterator it(input.begin(), input.end(), pattern);
    std::sregex_iterator end;

    // Extract all key-value pairs
    std::map<std::string, std::string> kv_pairs;
    for (; it != end; ++it) {
        std::string key   = (*it)[1];
        std::string value = (*it)[2];

        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        kv_pairs[key] = value;
    }

    if (kv_pairs.find("name") != kv_pairs.end()) {
        params.name = kv_pairs["name"];
    }
    if (kv_pairs.find("text") != kv_pairs.end()) {
        params.text = kv_pairs["text"];
    }
    if (kv_pairs.find("year") != kv_pairs.end()) {
        params.year = parseAndValidate("year", kv_pairs["year"], 1970, 2100);
    }
    if (kv_pairs.find("month") != kv_pairs.end()) {
        params.month = parseAndValidate("month", kv_pairs["month"], 1, 12);
    }
    if (kv_pairs.find("day") != kv_pairs.end()) {
        params.day = parseAndValidate("day", kv_pairs["day"], 1, 31);
    }

    return params;
}


struct PatternSegment {
    std::string text;
    bool        isWildcard;
};

class PrecompiledPattern {
private:
    std::vector<PatternSegment> segments;
    bool                        startsWithWildcard;
    bool                        endsWithWildcard;

public:
    PrecompiledPattern(std::vector<PatternSegment> segs, bool startWild, bool endWild)
    : segments(std::move(segs)),
      startsWithWildcard(startWild),
      endsWithWildcard(endWild) {}

    friend bool matchesPrecompiled(const std::string& line, const PrecompiledPattern& pattern);
};

std::unique_ptr<PrecompiledPattern> precompilePattern(const std::string& pattern) {
    std::vector<PatternSegment> segments;
    std::string                 currentSegment;
    bool                        startsWithWildcard = (!pattern.empty() && pattern[0] == '*');
    bool                        endsWithWildcard   = (!pattern.empty() && pattern.back() == '*');

    // Parse the pattern into segments
    for (char c : pattern) {
        if (c == '*') {
            if (!currentSegment.empty()) {
                segments.push_back({currentSegment, false});
                currentSegment.clear();
            }
            segments.push_back({"", true});
        } else {
            currentSegment += c;
        }
    }

    if (!currentSegment.empty()) {
        segments.push_back({currentSegment, false});
    }

    // Add wildcards at both ends if missing
    if (!startsWithWildcard) {
        segments.insert(segments.begin(), {"", true});
    }

    if (!endsWithWildcard) {
        segments.push_back({"", true});
    }

    return std::make_unique<PrecompiledPattern>(
        std::move(segments),
        true, // After adding, it always starts with wildcard
        true  // After adding, it always ends with wildcard
    );
}

bool matchesPrecompiled(const std::string& line, const PrecompiledPattern& pattern) {
    const auto& segments = pattern.segments;
    size_t      linePos  = 0;
    size_t      segIndex = 0;
    size_t      segCount = segments.size();

    // Handle empty line case
    if (line.empty()) {
        return false;
    }

    // Special case: pattern is just a wildcard
    if (segCount == 1 && segments[0].isWildcard) {
        return true;
    }

    // Match beginning (if not starting with wildcard)
    if (!pattern.startsWithWildcard) {
        const auto& firstSeg = segments[0];
        if (line.find(firstSeg.text) != 0) return false;
        linePos  = firstSeg.text.size();
        segIndex = 1;
    }

    // Match middle segments
    while (segIndex < segCount - (pattern.endsWithWildcard ? 0 : 1)) {
        const auto& seg = segments[segIndex];

        if (seg.isWildcard) {
            segIndex++;
            continue;
        }

        size_t foundPos = line.find(seg.text, linePos);
        if (foundPos == std::string::npos) return false;

        linePos = foundPos + seg.text.size();
        segIndex++;
    }

    // Match end (if not ending with wildcard)
    if (!pattern.endsWithWildcard) {
        const auto& lastSeg = segments.back();
        if (line.size() < lastSeg.text.size()
            || line.compare(line.size() - lastSeg.text.size(), lastSeg.text.size(), lastSeg.text) != 0) {
            return false;
        }
    }

    return true;
}

using StatusUpdate = std::function<void(const std::string&)>;

std::string searchForLogsInFile(
    LogSearchParams&               params,
    const telegram_bot::LogSource& source,
    const std::string&             filename,
    StatusUpdate&                  onUpdate
) {
    onUpdate("Reading " + filename);

    std::string   result;
    std::ifstream file(filename);
    std::string   line;

    if (!file.is_open()) {
        return "Error opening file";
    }

    int lineCount = 0;
    int each      = 100000;

    // Precompile the pattern once
    std::unique_ptr<PrecompiledPattern> compiledPattern;
    bool                                emptyPattern = true;

    if (!params.text.empty()) {
        emptyPattern    = false;
        compiledPattern = precompilePattern(params.text);
    }


    while (std::getline(file, line)) {
        if (lineCount > 0 && lineCount % each == 0) {
            if (lineCount >= each * 5) {
                each = each * 10;
            }
            onUpdate(
                "Reading " + filename + " line " + std::to_string(lineCount) + " found " + std::to_string(result.size())
                + " (update each " + std::to_string(each) + ")"
            );
        };
        if (lineCount % 100000 == 0) {
            telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
                "Reading " + filename + " line " + std::to_string(lineCount) + " found " + std::to_string(result.size())
            );
        }

        bool nameFilter = true;
        if (!params.name.empty()) {
            nameFilter = false;

            std::vector<std::string> fields;
            std::stringstream        ss(line);
            std::string              field;

            while (std::getline(ss, field, source.columnDelimeter[0])) {
                fields.push_back(field);
            }

            if (fields.size() >= source.nameColumnIndex + 1) {
                std::string& third_field = fields[source.nameColumnIndex];
                if (third_field == params.name) nameFilter = true;
            }
        }
        bool match = emptyPattern || matchesPrecompiled(line, *compiledPattern);

        if (nameFilter && match) {
            result += "\n" + line;
        }

        // size is expensive, calculate each 10 lines
        if (lineCount % 10 == 0 && utf8Size(result) > PAGE_LIMIT * source.maxPages) {
            result += "\n\nEND";
            break;
        }

        lineCount++;
    }

    return result;
}

struct LogsSearchResult {
    std::string result;
    std::string info;
};

std::string replaceFileFormat(const std::string& format, const LogSearchParams& params) {
    auto msg = std::string(format);
    telegram_bot::Utils::replaceString(msg, "{year}", std::to_string(params.year));
    telegram_bot::Utils::replaceString(
        msg,
        "{month}",
        params.month < 10 ? "0" + std::to_string(params.month) : std::to_string(params.month)
    );
    telegram_bot::Utils::replaceString(
        msg,
        "{day}",
        params.day < 10 ? "0" + std::to_string(params.day) : std::to_string(params.day)
    );
    return msg;
};

LogsSearchResult
searchForLogsInSource(LogSearchParams& params, telegram_bot::LogSource& source, StatusUpdate& onUpdate) {
    std::filesystem::path directoryPath    = source.folder;
    std::string           expectedFileName = replaceFileFormat(source.fileFormat, params);
    std::string           info             = "searching for '" + expectedFileName + "', files:\n";

    telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
        "searching for: {} in {}",
        expectedFileName,
        source.folder
    );

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (!entry.is_regular_file()) continue;

        auto filename  = entry.path().filename().string();
        info          += filename + "\n";
        telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info("found: " + filename);

        if (filename == expectedFileName) {
            return {.result = searchForLogsInFile(params, source, entry.path().string(), onUpdate), .info = ""};
        }
    };

    return {.result = "", .info = info};
}

LogsSearchResult searchForLogs(LogSearchParams& params, StatusUpdate& onUpdate) {
    std::string result;
    std::string info;
    for (auto source : telegram_bot::config.logsSearchCommand.logSources) {
        auto searchResult  = searchForLogsInSource(params, source, onUpdate);
        result            += searchResult.result;
        info              += searchResult.info;
    };
    return {.result = result, .info = info};
}


struct BrowseState {
    std::string  results;
    std::string  uuid;
    int          page;
    int          maxPages;
    std::int64_t chatId;
    std::int32_t messageId;
};

#define DEL  "0"
#define NEXT "1"
#define PREV "2"


namespace telegram_bot::tgcommands {

std::unordered_map<std::string, BrowseState> browseStates;

void applyBrowseState(TgBot::Bot& bot, const BrowseState& state) {
    size_t start = state.page * (PAGE_LIMIT);
    size_t end   = (PAGE_LIMIT);
    auto   text = "```\n" + telegram_bot::Utils::escapeStringForTelegram(utf8Substr(state.results, start, end)) + "```";

    using namespace TgBot;

    InlineKeyboardMarkup::Ptr              keyboard(new InlineKeyboardMarkup);
    std::vector<InlineKeyboardButton::Ptr> row0;
    InlineKeyboardButton::Ptr              btn(new InlineKeyboardButton);
    btn->text         = "<";
    btn->callbackData = state.uuid + PREV;
    row0.push_back(btn);

    btn               = std::make_shared<InlineKeyboardButton>();
    btn->text         = std::to_string(state.page) + "/" + std::to_string(state.maxPages);
    btn->callbackData = " empty ";
    row0.push_back(btn);

    if (state.page < state.maxPages) {
        btn               = std::make_shared<InlineKeyboardButton>();
        btn->text         = ">";
        btn->callbackData = state.uuid + NEXT;
        row0.push_back(btn);
    }


    keyboard->inlineKeyboard.push_back(row0);

    std::vector<InlineKeyboardButton::Ptr> row1;
    btn               = std::make_shared<InlineKeyboardButton>();
    btn->text         = "Close";
    btn->callbackData = state.uuid + DEL;
    row1.push_back(btn);
    keyboard->inlineKeyboard.push_back(row1);


    bot.getApi().editMessageText(text, state.chatId, state.messageId, "", "MarkdownV2", nullptr, keyboard);
}

void logsSearchInit(TgBot::Bot& bot, const TgBot::Message::Ptr& message, const std::string& params) {
    auto parsed = parseLogSearchParams(params);
    if (parsed.name.empty() && parsed.text.empty()) {
        return telegram_bot::reply(bot, message, "No params to search!");
    };

    auto smessage = bot.getApi().sendMessage(
        message->chat->id,
        "Executing logs search with params " + params,
        nullptr,
        nullptr,
        nullptr,
        "",
        false,
        {},
        message->isTopicMessage ? message->messageThreadId : 0
    );

    std::string  lastText;
    StatusUpdate onUpdate = [&bot, &smessage, &lastText](const std::string& text) {
        if (text == lastText) return;
        lastText = text;
        bot.getApi().editMessageText(text, smessage->chat->id, smessage->messageId);
    };

    try {
        auto search = searchForLogs(parsed, onUpdate);

        if (search.result.empty()) {
            onUpdate("No logs found\n" + utf8Substr(search.info, 0, 1000));
        } else {
            std::string uuid = getUuid();

            BrowseState state{
                .results   = search.result,
                .uuid      = uuid,
                .page      = 0,
                .maxPages  = static_cast<int>(utf8Size(search.result) / PAGE_LIMIT),
                .chatId    = smessage->chat->id,
                .messageId = smessage->messageId
            };

            applyBrowseState(bot, state);
            TelegramBotMod::getInstance().getSelf().getLogger().info("set uuid {}", uuid);

            browseStates[uuid] = state;
        }
    } catch (const std::exception& e) {
        auto error = std::string(e.what());
        TelegramBotMod::getInstance().getSelf().getLogger().error("telegram search logs error: " + error);
        onUpdate("Logs search failed: " + error);
    }
}

void logs(TgBot::Bot& bot) {
    if (!config.logsSearchCommand.enabled) return;

    add(
        {.name        = "logs",
         .description = "Use without params to get list of search params",
         .adminOnly   = true,
         .listener =
             [&bot](const TgBot::Message::Ptr& message) {
                 auto params = getParams(message->text);
                 if (params.empty()) {
                     telegram_bot::reply(
                         bot,
                         message,
                         "Use these params:\nname - Name of the player\ntext - Text of the log\nday - Day of the "
                         "month\nmonth - Month\nyear - Year\n\nExamples:\n/logs "
                         "name=leaftail1880 text=\"some text\""
                     );
                 } else {
                     try {
                         logsSearchInit(bot, message, params);
                     } catch (const std::exception& e) {
                         auto error = std::string(e.what());
                         TelegramBotMod::getInstance().getSelf().getLogger().error(
                             "telegram search logs error: " + error
                         );
                         telegram_bot::reply(bot, message, "Logs search init failed: " + error);
                     }
                 }
             }}
    );

    bot.getEvents().onCallbackQuery([&bot](const TgBot::CallbackQuery::Ptr& query) {
        try {
            TelegramBotMod::getInstance().getSelf().getLogger().info("callbackQuery data: {}", query->data);

            std::string uuid   = query->data.substr(0, query->data.size() - 1);
            char        action = query->data[query->data.size() - 1];


            auto& state = browseStates.at(uuid);


            switch (action) {
            case DEL[0]:
                browseStates.erase(uuid);
                bot.getApi().deleteMessage(state.chatId, state.messageId);
                return;
            case NEXT[0]:
                state.page = std::min(state.page + 1, state.maxPages);
                break;
            case PREV[0]:
                state.page = std::max(state.page - 1, 0);
                break;
            default:
                throw std::exception(("unknown action: " + std::string(1, action)).c_str());
                break;
            };

            applyBrowseState(bot, state);
            bot.getApi().answerCallbackQuery(query->id);
        } catch (const std::exception& e) {
            auto error = std::string(e.what());
            TelegramBotMod::getInstance().getSelf().getLogger().error("telegram browse logs error: " + error);
            bot.getApi().answerCallbackQuery(query->id, error, true);
        }
    });
};

void cleanLogsStorage() { browseStates.clear(); }

} // namespace telegram_bot::tgcommands