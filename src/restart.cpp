#include <thread>
#include "watcher.hpp"
#include "aggregator.hpp"
#include "settings.hpp"
#include "logger.hpp"
#include "restart.hpp"


std::atomic<bool> Logwatch::Restart::apply_inprogress{ false };
std::atomic<bool> Logwatch::Restart::apply_done{ false };

void Logwatch::Restart::discardBackupWhenWatcherReady() {
	while (watcher.isWarmingUp()) std::this_thread::sleep_for(std::chrono::milliseconds(25));
	aggr.invalidateBackup();
}

void Logwatch::Restart::RestartTask::operator()() const {
	try {
		logger::info("Restarting the watcher.");
		watcher.nudge();
		watcher.stop();

		if (enableDeepScan) {
			aggr.backupAndClear();
			watcher.clear();
			watcher.startLogWatcher();
		}
		else if (disableDeepScan) {
			watcher.clear();
			aggr.restoreAndClear();
			watcher.startLogWatcher();
			std::jthread(discardBackupWhenWatcherReady).detach();
		}
		else {
			watcher.clear();
			aggr.clear();
			watcher.startLogWatcher();
		}

		apply_done.store(true, std::memory_order_relaxed);
		logger::info("Watcher restart completed.");
	}
	catch (const std::exception& e) {
		logger::error("Could not restart the watcher: {}", e.what());
		apply_done.store(false, std::memory_order_relaxed);
	}
	catch (...) {
		logger::error("Could not restart the watcher due to an unknown error.");
		apply_done.store(false, std::memory_order_relaxed);
	}

	apply_inprogress.store(false, std::memory_order_relaxed);
}

bool Logwatch::Restart::restartWatcher(const LogWatcherSettings& st, const Config& prev_config) {

	bool expected = false;
	if (!apply_inprogress.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		logger::info("The watcher is already restarting; ignored another Apply request.");
		return false; // already running
	}

	apply_done.store(false, std::memory_order_relaxed);

	const bool toDeep = st.deepScan && !prev_config.deepScan;
	const bool fromDeep = !st.deepScan && prev_config.deepScan;

	std::jthread(RestartTask(toDeep, fromDeep)).detach();

	return true;
}
