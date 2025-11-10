#include <thread>
#include "watcher.hpp"
#include "aggregator.hpp"
#include "settings.hpp"
#include "logger.hpp"
#include "restart.hpp"


std::atomic<bool> Logwatch::Restart::apply_inflight{ false };
std::atomic<bool> Logwatch::Restart::apply_done{ false };

bool Logwatch::Restart::restartWatcher(const LogWatcherSettings& st, const Config& prev_config) {

	bool expected = false;
	if (!apply_inflight.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
		logger::info("Restart already in flight; ignoring duplicate Apply.");
		return false; // already running
	}

	apply_done.store(false, std::memory_order_relaxed);

	const bool toDeep = st.deepScan && !prev_config.deepScan;
	const bool fromDeep = !st.deepScan && prev_config.deepScan;

	// Launch async restart (we got to be careful here, freaking this up will mess the watcher)
	std::jthread([toDeep, fromDeep]() mutable {
		try {
			logger::info("Restarting Log Watcher asynchronously");

			// Wake the worker so stop() doesn't wait a full poll period
			watcher.nudge();

			// Stop Watcher and join
			watcher.stop();

			if (toDeep) {				
				aggr.backupAndClear();
				watcher.clear();
				watcher.startLogWatcher();
			}
			else if (fromDeep) { 
				watcher.clear();
				aggr.restoreAndClear();
				watcher.startLogWatcher();
				// wait until fully live this kisses the backup goodbye
				std::jthread([] {
					while (Logwatch::watcher.isWarmingUp()) {
						std::this_thread::sleep_for(std::chrono::milliseconds(25));
					}
					aggr.invalidateBackup();
				}).detach();
			}
			else {
				watcher.clear();
				aggr.clear();
				watcher.startLogWatcher();
			}

			apply_done.store(true, std::memory_order_relaxed);
			logger::info("Restart complete");
		}
		catch (const std::exception& e) {
			logger::error("Restart failed: {}", e.what());
			apply_done.store(false, std::memory_order_relaxed);
		}
		catch (...) {
			logger::error("Restart failed: why so serious?");
			apply_done.store(false, std::memory_order_relaxed);
		}

		apply_inflight.store(false, std::memory_order_relaxed);

	}).detach();

	return true;
}