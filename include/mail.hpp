#pragma once

#include <string>
#include <vector>

#include "notification.hpp"

namespace Logwatch {

	enum class MailType : uint8_t { PeriodicAlert, PinnedAlert };

    struct MailModDiff {
        std::string mod;
        uint64_t errors = 0;
        uint64_t warnings = 0;
        uint64_t fails = 0;
    };

    struct EntryDiff {
        std::string mod;
        Counts counts;
        uint64_t levelCount;
    };

    struct MailEntry {
        MailType type;
        std::string title;
        std::string summary;
        std::chrono::system_clock::time_point when;
        std::vector<MailModDiff> mods;
    };

    struct MailBox {
        std::deque<MailEntry> q;
        size_t cap { 200 };
    };

} 
