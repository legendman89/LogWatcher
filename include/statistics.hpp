#pragma once

#include <deque>
#include <string>
#include <chrono>
#include <unordered_map>

namespace Logwatch {

    enum Level : uint8_t { kError = 1, kWarning = 2, kFail = 4, kOther = 8 };

    struct ModStats {

        // TODO: this should be in Counts.
        int errors = 0;
        int warnings = 0;
        int fails = 0;
        int others = 0;

        struct Record {
            std::string level;
            std::string file;
            std::string text;
            uint64_t lineNo;
            std::chrono::system_clock::time_point when;
            uint8_t levelMask = Level::kOther;
        };

        std::deque<Record> last;  // ring buffer
    };

    using Snapshot = std::unordered_map<std::string, ModStats>;

}
