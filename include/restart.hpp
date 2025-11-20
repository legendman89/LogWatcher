#pragma once

#include <atomic>
#include "settings.hpp"
#include "config.hpp"

namespace Logwatch::Restart {

    extern std::atomic<bool> apply_inprogress;
    extern std::atomic<bool> apply_done;

    inline bool ApplyOnTheFly() { return apply_inprogress.load(std::memory_order_relaxed); }
    inline bool ApplyDone() { return apply_done.load(std::memory_order_relaxed); }

    bool restartWatcher(const LogWatcherSettings& st, const Config& prev_config);
}
