#pragma once

#include "settings_def.hpp"
#include "config.hpp"

namespace Logwatch {

    inline bool operator==(const LogWatcherSettings& a, const LogWatcherSettings& b) {

        #define SETTINGS2EQ(S, D) a.S == b.S &&

        return FOREACH_BOOL_SETTING(SETTINGS2EQ) FOREACH_SIZE_SETTING(SETTINGS2EQ) FOREACH_FLT_SETTING(SETTINGS2EQ) true;

    }

    inline bool operator!=(const LogWatcherSettings& a, const LogWatcherSettings& b) { return !(a == b); }

    inline LogWatcherSettings& GetSettings() { static LogWatcherSettings st; return st; }

    void applyNow();

    void loadDefaults(const LogWatcherSettings& factory);

	bool restartRequired(const LogWatcherSettings& curr, const Config& prev);

}
