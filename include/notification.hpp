#pragma once

#include <chrono>
#include <deque>

#include "state.hpp"

namespace Logwatch {

    // TODO: move this to statistics to avoid maintaining duplicate code.
    struct Counts {
        uint64_t errors = 0;
        uint64_t warnings = 0;
        uint64_t fails = 0;
        uint64_t others = 0;
    };

    struct PinnedSnapshot {
        Counts counts;
        Clock::time_point lastAlertAt{};
    };

    struct MessageQueue {
        std::deque<std::string> q;
        Clock::time_point nextAt{};
    };

} 
