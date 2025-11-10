#pragma once

#include <string>
#include <chrono>
#include <cstdint>
#include <filesystem>

namespace Logwatch {

    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;

    enum class LogType { Generic, Papyrus };

    struct Match {
        std::string file;           // Full path of the log file
        std::string line;           // The matched line
        std::string keyword;        // Which keyword/regex matched
        std::string level;          // error/warning/fail/other
        uint64_t lineNo;            // 1-based line number
        std::chrono::system_clock::time_point when; // timestamp 
    };

    struct TailState {
        uint64_t offset = 0;             // byte offset we consumed so far
        uint64_t lineNo = 0;             // line counter
        uint64_t sizeLastSeen = 0;       // last known file size
        fs::file_time_type writeTime{};  // last write time
        Clock::time_point lastPoll{};
    };

    struct FileInfo {
        fs::path path;
        TailState state;
        LogType type = LogType::Generic;
    };


}
