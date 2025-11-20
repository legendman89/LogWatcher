
#include "watcher.hpp"
#include "aggregator.hpp"
#include "settings.hpp"
#include "settings_json.hpp"
#include "restart.hpp"

void Logwatch::LoadDefaults(const LogWatcherSettings& factory) {
	auto& st = Settings();
    st = factory;
	logger::info("Settings reset to defaults");
}

// Implement my restart rules
bool Logwatch::restartRequired(const LogWatcherSettings& curr, const Config& prev) {
	if (curr.deepScan != prev.deepScan) return true;
	if (curr.watchPapyrus != prev.watchPapyrus) return true;
	return false;
}

void Logwatch::ApplyNow() {
    auto& st = Settings();
    auto& config = watcher.configurator();
	const auto prev_config = config; // copy for comparison

#define SETTING2CONFIG(S, D) config.S = st.S;

    FOREACH_BOOL_SETTING(SETTING2CONFIG);
    FOREACH_SIZE_SETTING(SETTING2CONFIG);
    FOREACH_FLT_SETTING(SETTING2CONFIG);

    // Manual adjustment
    config.pollIntervalMs = std::clamp(st.pollIntervalMs, 100, 5000);
    config.pollInterval = std::chrono::milliseconds{ config.pollIntervalMs };

    aggr.setCapacity((size_t)st.cacheCap);

    Logwatch::Restart::apply_done.store(false, std::memory_order_relaxed);
    Logwatch::Restart::apply_inprogress.store(false, std::memory_order_relaxed);

    const bool needRestart = restartRequired(st, prev_config);
    logger::info("Is restart needed to apply settings? {}", needRestart);

    if (!needRestart) {

        logger::info("Applying new configuration without restarting watcher");
        config.print();

        watcher.nudge();

		// Signal UI to show done status
        Logwatch::Restart::apply_done.store(true, std::memory_order_relaxed);
    }
    else {
        Restart::restartWatcher(st, prev_config);
    }

    Logwatch::settingsPersister.saveStateAsync();
}