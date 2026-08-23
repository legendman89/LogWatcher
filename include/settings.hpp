#pragma once

#include <mutex>

#include "settings_def.hpp"
#include "config.hpp"

namespace Logwatch {

    inline bool operator==(const LogWatcherSettings& a, const LogWatcherSettings& b) {

        #define SETTINGS2EQ(S, D) a.S == b.S &&

        return FOREACH_BOOL_SETTING(SETTINGS2EQ) FOREACH_SIZE_SETTING(SETTINGS2EQ) FOREACH_FLT_SETTING(SETTINGS2EQ) true;

    }

    inline bool operator!=(const LogWatcherSettings& a, const LogWatcherSettings& b) { return !(a == b); }

    class SettingsStore {

    private:

        mutable std::mutex _mutex_;

        LogWatcherSettings settings;

    public:

        LogWatcherSettings read() const;

        void replace(const LogWatcherSettings& replacement);
    };

    inline SettingsStore& GetSettingsStore() {
        static SettingsStore store;
        return store;
    }

    inline LogWatcherSettings ReadSettings() { return GetSettingsStore().read(); }

    inline void SetSettings(const LogWatcherSettings& settings) { GetSettingsStore().replace(settings); }

    void applyNow();

    void loadDefaults(const LogWatcherSettings& factory);

	bool restartRequired(const LogWatcherSettings& curr, const Config& prev);

}
