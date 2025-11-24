#pragma once

#include <vector>
#include <regex>
#include <string>
#include <chrono>
#include "settings_def.hpp"
#include "logger.hpp"

namespace Logwatch {

#define SIZE2CONFIG(S, D)  size_t S = D;

    struct Config {
        
        FOREACH_BOOL_SETTING(BOOL2DEF);
        FOREACH_SIZE_SETTING(SIZE2CONFIG);
        FOREACH_FLT_SETTING(FLT2DEF);

        std::regex includeFileRegex;
        std::regex excludeFileRegex;
        std::vector<std::pair<std::string, std::regex>> patterns;

        // Manual conversion (must be ouside code generation above)
        std::chrono::milliseconds pollInterval{ std::chrono::milliseconds{pollIntervalMs} };

        Config() try
            : includeFileRegex(R"((?:^|[\\/]).+\.(?:log)$)", std::regex::icase)
            , excludeFileRegex(R"((^|[\\/])crash-\d{4}-\d{2}-\d{2}-\d{2}-\d{2}-\d{2}\.log$)", std::regex::icase)
            , patterns{
                {"error", 
                    std::regex(
                      R"((\[\s*(error|e|critical|crit)\s*\])"
                      R"(|\(\s*(error|e|critical|crit)\s*\))"
                      R"(|(^|\s)(ERROR|ERR|CRITICAL|CRIT)\b)"
                      R"(|(^|\s)error:)"
                      R"(|(^|\s)critical:))",
                    std::regex::icase)
                },
                {"warning", 
                    std::regex(
                      R"((\[\s*warn(?:ing)?\s*\])"
                      R"(|\(\s*warn(?:ing)?\s*\))"
                      R"(|(^|\s)WARN(?:ING)?\b)"
                      R"(|(^|\s)warning:))",
                    std::regex::icase)
                },
                {"fail",    std::regex(R"((\bfail(?:ed|ure)?\b|\[\s*fail(?:ed|ure)?\s*\]))", std::regex::icase)},
                {"other",   std::regex(R"(.+)", std::regex::ECMAScript)}
            }
        {
            pollInterval = std::chrono::milliseconds{ pollIntervalMs };
        }
        catch (const std::regex_error&) {
            includeFileRegex = std::regex(R"(.+)", std::regex::ECMAScript);
            excludeFileRegex = std::regex(R"(^$)", std::regex::ECMAScript);
            patterns.clear();
            patterns.emplace_back("other", std::regex(R"(.+)", std::regex::ECMAScript));
        }

        void loadFromSettings(const LogWatcherSettings& st) {
            #define SETTING2CONFIG(S, D) S = st.S;
            FOREACH_BOOL_SETTING(SETTING2CONFIG);
            FOREACH_SIZE_SETTING(SETTING2CONFIG);
            FOREACH_FLT_SETTING(SETTING2CONFIG);
			pollInterval = std::chrono::milliseconds{ pollIntervalMs };
        }

        void print() const {
            #define FLTSETTING2PRINT(S, D) logger::info("  {:30s} : {:.2f}", #S, S);
            #define SIZESETTING2PRINT(S, D) logger::info("  {:30s} : {}", #S, S);
            #define BOOLSETTING2PRINT(S, D) logger::info("  {:30s} : {}", #S, S ? "true" : "false");
            FOREACH_BOOL_SETTING(BOOLSETTING2PRINT);
            FOREACH_SIZE_SETTING(SIZESETTING2PRINT);
            FOREACH_FLT_SETTING(FLTSETTING2PRINT);
        }
    };


}
