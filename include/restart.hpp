#pragma once

#include <atomic>
#include "settings.hpp"
#include "config.hpp"

namespace Logwatch::Restart {

    class RestartTask {

    private:

        bool enableDeepScan;
        bool disableDeepScan;

    public:

        RestartTask(const bool toDeepScan, const bool fromDeepScan) : 
            enableDeepScan(toDeepScan), disableDeepScan(fromDeepScan) {}

        void operator()() const;
    };

    extern std::atomic<bool> apply_inprogress;
    extern std::atomic<bool> apply_done;

    inline bool ApplyOnTheFly() { return apply_inprogress.load(std::memory_order_relaxed); }
    inline bool ApplyDone() { return apply_done.load(std::memory_order_relaxed); }

    void discardBackupWhenWatcherReady();

    bool restartWatcher(const LogWatcherSettings& st, const Config& prev_config);
}
