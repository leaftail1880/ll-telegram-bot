#include "Logs.h"
#include "TelegramBot/Config.h"
#include "TelegramBot/TelegramBot.h"
#include "TelegramBot/Utils.h"
#include "TelegramBot/telegram/TgUtils.h"
// #include "csv.hpp"
#include <chrono>
#include <map>
#include <regex>
#include <string>
#include <utf8.h>

#define TG_MESSAGE_LIMIT 4096

void fix_utf8_string(std::string& str) {
    std::string temp;
    utf8::replace_invalid(str.begin(), str.end(), back_inserter(temp));
    str = temp;
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


bool isLogField(LogSearchParams& params, const std::basic_string<char>& name, const std::string& value) {
    if (!params.name.empty()) {
        if (name == "Имя игрока") {
            if (params.name == value) return true;
        }
    }

    return false;
}

using StatusUpdate = std::function<void(const std::string&)>;


std::string searchForLogsInFile(LogSearchParams& params, const std::string& filename, StatusUpdate& onUpdate) {
    onUpdate("Reading " + filename);

    std::string   result;
    std::ifstream file(filename);
    std::string   line;

    if (!file.is_open()) {
        return "Error opening file";
    }

    std::regex pattern("");
    bool       patternEmpty = true;
    if (!params.text.empty()) {
        patternEmpty = false;
        pattern      = std::regex(params.text);
    }

    int line_count = 0;
    while (std::getline(file, line)) {
        if (line_count % 1000000 == 0) {
            telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
                "Reading " + filename + " line "
                + std::to_string(line_count) // + " read " + std::to_string(result.size()
            );
        }
        bool nameFound = (params.name.empty() || line.find("," + params.name + ",") != std::string::npos);
        bool match     = patternEmpty || std::regex_match(line, pattern);
        if (nameFound && match) {
            result += line + "\n";
        }
        line_count++;

        if (result.size() > TG_MESSAGE_LIMIT * 50) {
            result += "\nTRUNKATED";
            break;
        }
    }

    // CSVReader    reader(filename);
    // std::int64_t rowI = 0;
    // CSVRow       row;
    // bool         need = true;
    // while (need) {
    //     if (rowI > 1749077) {
    //         telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
    //             "Reading " + filename + " line " + std::to_string(rowI) // + " read " + std::to_string(result.size()
    //         );
    //     }
    //     need = reader.read_row(row);
    //     if (rowI > 1749077) {
    //         telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
    //             "Reading " + filename + " line " + std::to_string(rowI) + " done "
    //             + (need ? "yes" : "no") // + " read " + std::to_string(result.size()
    //         );
    //     }
    //     rowI++;
    // };

    // std::string result;

    // std::vector<std::string> colNames;

    // int64        each = 100000;

    // std::regex textSearch("");
    // bool       textSearchEnabled = false;
    // if (!params.text.empty()) {
    //     textSearchEnabled = true;
    //     textSearch        = std::regex(params.text);
    // }

    // std::ofstream ost{"test.txt", std::ios_base::app};

    // for (CSVRow& row : reader) { // Input iterator
    // ost << "\n" + std::to_string(rowI) + " ";

    // if (colNames.empty()) colNames = row.get_col_names();
    // int i = 0;

    // std::string rowContent;
    // bool        fieldsMatch = false;

    // if (rowI > 0 && rowI % each == 0) {
    //     if (rowI >= each * 5) {
    //         each = each * 5;
    //     }
    //     onUpdate(
    //         "Reading " + filename + " line " + std::to_string(rowI) + " read " + std::to_string(result.size())
    //         + " (update each " + std::to_string(each) + ")"
    //     );
    // };
    // if (rowI % 10000 == 0) {
    //     telegram_bot::TelegramBotMod::getInstance().getSelf().getLogger().info(
    //         "Reading " + filename + " line " + std::to_string(rowI) // + " read " + std::to_string(result.size()
    //     );
    //     rowI = 0;
    // }

    // for (CSVField& field : row) {
    //     auto& colName = colNames[i];
    //     i++;
    //     std::string content  = field.get<std::string>();
    //     rowContent          += " :" + content;


    //     if (isLogField(params, colName, content)) {
    //         fieldsMatch = true;
    //     }
    // }

    // ost << rowContent;

    // auto rowMatch = (textSearchEnabled ? std::regex_match(rowContent, textSearch) : true);
    // if (fieldsMatch && rowMatch) {
    //     // add header if empty
    //     if (result.empty()) {
    //         for (auto& colName : colNames) {
    //             result += colName;
    //             result += " ";
    //         }
    //         result += "\n";
    //     }

    //     result += "\n" + rowContent;
    // }

    // ost << "  result size: " + std::to_string(result.size());

    // rowI++;


    // }

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
            return {.result = searchForLogsInFile(params, entry.path().string(), onUpdate), .info = ""};
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


namespace telegram_bot::tgcommands {

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

    StatusUpdate onUpdate = [&bot, &smessage](const std::string& text) {
        bot.getApi().editMessageText(text, smessage->chat->id, smessage->messageId);
    };

    try {
        auto search = searchForLogs(parsed, onUpdate);

        if (search.result.empty()) {
            telegram_bot::reply(bot, message, "No logs found\n" + search.info.substr(0, 1000));
        } else {


            TelegramBotMod::getInstance().getSelf().getLogger().info("telegram runCommand res: " + search.result);
            fix_utf8_string(search.result);
            telegram_bot::reply(
                bot,
                message,
                "```" + Utils::escapeStringForTelegram(search.result.substr(0, TG_MESSAGE_LIMIT)) + "```",
                "MarkdownV2"
            );
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
};


} // namespace telegram_bot::tgcommands