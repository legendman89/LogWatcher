#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <vector>

#include "notification.hpp"

namespace Logwatch {

	enum class MailType : uint8_t { PeriodicAlert, PinnedAlert };

    struct MailModDiff {
        std::string mod;

        Counts counts;
    };

    struct EntryDiff {
        std::string mod;

        uint64_t levelCount;

        Counts counts;
    };

    class EntryDiffMore {

    public:

        inline bool operator()(const EntryDiff& left, const EntryDiff& right) const {
            return left.levelCount > right.levelCount;
        }
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
