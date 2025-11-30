#pragma once

#include <atomic>
#include <fstream>
#include <functional>
#include <mutex>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>
#include <condition_variable>
#include <stop_token>
#include <chrono>

#include "state.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "plugin.hpp"
#include "statistics.hpp"
#include "mail.hpp"

namespace Logwatch {

    // State for the pause/resume.
    enum class RunState { Running, AutoStopPending, Stopped };

    // Matches a log line.
    void OnMatch(const Match& m);

    // Papyrus or Generic?
    LogType classify(const std::filesystem::path& p);

    // For the OnMatach or any other callable.
    using Callback = std::function<void(const Match&)>;

    class LogWatcher {
    private:

        // Watcher thread. Must be jthread, std::thread won't help for shit here.
        // My first attempt was std::thread, and was going into limbo not able
        // to figure out how to send stop signals.
        std::jthread worker;

		// Run state for auto-stop button.
        std::atomic<RunState> runState{ RunState::Running };
        std::atomic<bool> firstPollDone{ false };

		// Warming up overlay state.
        std::atomic<bool> warming_up{ true };

        // For locks. Mutable for the same reason as the aggregator.
        mutable std::mutex _mutex_;

        // Sleep/wakeup for the worker
        std::mutex                  _wake_mutex_;

        // This is one reason why I needed jthread.
        std::condition_variable_any _wake_cv_;

        Config   config;
        Callback callback;

        // Bookkeeping.
        std::vector<fs::path>                         roots;
        std::unordered_map<std::string, FileInfo>     files;

        // Delay notifications until save is loaded.
        std::atomic<bool> gameReady{ false };
        std::atomic<Clock::time_point> hudStartDelay{ Clock::time_point{} };

        // Periodic summary.
        Counts periodicLastTotals{};
        Clock::time_point periodicNextAt{ };
        bool periodicReady{ false };
        std::unordered_map<std::string, Counts> periodicLastPerMod;
        
        // Pinned alerts.
        std::unordered_map<std::string, PinnedSnapshot> pinnedState;
        std::deque<HUDMessage> hudMessages;

        // Mail.
        MailBox mailbox;

        // Worker body.
        void watcherLoop(const std::stop_token& stop);

        // Scan functions
        void scanOnce(const std::stop_token& stop);
        bool shouldInclude(const fs::path& file) const;
        void discoverFiles(std::vector<fs::path>& out, const fs::path& root, const std::stop_token& stop);
        void tailFile(FileInfo& fi, const std::stop_token& stop);

        // TODO: make chunk constant.
        void parseBufferAndEmit(FileInfo& fi, std::string&& chunk, const std::stop_token& stop);

        // Line here has to be string_view to avoid reallocation.
        void emitIfMatch(const fs::path& file, const std::string_view& line, const uint64_t& lineNo);

        // Notification functions
        void updatePeriodicBase(const Snapshot& snap, const Clock::time_point& now, const int& interval);
        void mayNotifyPinnedAlerts(const Snapshot& snap);
        void mayNorifyPeriodicAlerts(const Snapshot& snap);

        inline void scheduleNotification(HUDMessage&& m) {
            std::lock_guard lock(_mutex_);
            hudMessages.push_back(std::move(m));
        }

        inline void scheduleMail(MailEntry&& e) {
            std::lock_guard lock(_mutex_);
            mailbox.q.push_back(std::move(e));
            if (mailbox.q.size() > mailbox.cap)
                mailbox.q.pop_front();
        }

        inline uint64_t levelCount(const Counts& c, const int& minLevel) {
            switch (minLevel) {
                case 0:
                    return c.errors;
                case 1:
                    return c.errors + c.warnings;
                default:
                    return c.errors + c.warnings + c.fails;
            }
        }

        inline void updatePinnedBase(const std::string& modKey, const Counts& curr, const Clock::time_point& now) {
            PinnedSnapshot ps;
            ps.counts = curr;
            ps.lastAlertAt = now;
            pinnedState[modKey] = ps;
        }

        inline void computeDiff(Counts& d, const Counts& curr, const Counts& prev) {
            d.errors = (curr.errors > prev.errors) ? (curr.errors - prev.errors) : 0;
            d.warnings = (curr.warnings > prev.warnings) ? (curr.warnings - prev.warnings) : 0;
            d.fails = (curr.fails > prev.fails) ? (curr.fails - prev.fails) : 0;
            d.others = (curr.others > prev.others) ? (curr.others - prev.others) : 0;
        }

    public:
        LogWatcher() = default;
        ~LogWatcher() { stop(); }

        void startLogWatcher();

        void addLogDirectories();

        void addIfExists(const fs::path& p);

        inline bool isHUDQueueEmpty() const noexcept { return hudMessages.empty(); }

        inline void popHUDMessage(HUDMessage& hudMessage) {
            hudMessage = std::move(hudMessages.front());
            hudMessages.pop_front();
        }

        inline void setHUDStartDelay(const size_t& delay) noexcept { 
            hudStartDelay.store(Clock::now() + std::chrono::seconds(delay), std::memory_order_relaxed);
        }

        inline bool isHUDTimeReady() const noexcept { 
            const auto time = hudStartDelay.load(std::memory_order_relaxed);
            if (!time.time_since_epoch().count()) return false;
            return Clock::now() >= time;
        }

        inline void setGameReady(const bool& val) noexcept { gameReady.store(val, std::memory_order_relaxed); }

        inline bool isGameReady() const noexcept { return gameReady.load(std::memory_order_relaxed); }

        inline bool isWarmingUp() const noexcept { return warming_up.load(std::memory_order_relaxed); }

        inline Config& configurator() { return config; }
        inline const Config& configurator() const { return config; }

        inline void addDirectory(const fs::path& dir) {
            std::lock_guard lock(_mutex_);
            roots.push_back(dir);
        }

        inline void setCallback(Callback cb) {
            std::lock_guard lock(_mutex_);
            callback = std::move(cb);
        }

        inline void start() {
            if (worker.joinable()) {
                logger::warn("Watcher thread is already running; start ignored");
                return;
            }
            logger::info("Starting Watcher thread");
            worker = std::jthread(
                [this](const std::stop_token& st) { 
                    try {
                        watcherLoop(st);
                    }
                    catch (const std::exception& e) {
                        logger::error("Unhandled exception in Watcher thread: {}", e.what());
                    }
                    catch (...) {
                        logger::error("Unknown unhandled exception in Watcher thread");
					}
                }
            );
        }

        inline void stop() {
            if (!worker.joinable()) return;
            logger::info("Stopping Watcher thread");
            worker.request_stop();
            nudge(); // can wake any sleeping worker
            worker.join();
        }

        inline void setRunState(const RunState& rs) {
            runState.store(rs, std::memory_order_relaxed);
            nudge(); // wake UI thread if needed
		}

        inline RunState getRunState() const {
			return runState.load(std::memory_order_relaxed);
		}

        inline void checkRunState() {
            if (config.pauseWatcher) {
                logger::info("Watcher is paused per settings; pausing after first poll");
                runState = RunState::AutoStopPending;
            }
            else {
				runState = RunState::Running;
            }
		}

        void clear() {
            std::lock_guard lock(_mutex_);
            files.clear();
        }

        inline size_t discoveredFileCount() {
            std::lock_guard lock(_mutex_);
            return files.size();
        }

        inline void nudge() {
            _wake_cv_.notify_all();
        }

        inline void resetNotifications() {
            periodicReady = false;
            periodicLastPerMod.clear();
            periodicLastTotals = {};
            pinnedState.clear();
        }

        std::vector<Logwatch::MailEntry> snapshotMailbox() const {
            std::lock_guard lock(_mutex_);
            std::vector<MailEntry> out;
            out.reserve(mailbox.q.size());
            for (const auto& m : mailbox.q)
                out.push_back(m);
            return out;
        }

        inline void resetFirstPoll() noexcept {
            firstPollDone.store(false, std::memory_order_relaxed);
		}

        inline void markFirstPollDone() noexcept {
			firstPollDone.store(true, std::memory_order_relaxed);
		}

        inline bool isFirstPollDone() const noexcept {
			return firstPollDone.load(std::memory_order_relaxed);
		}

        inline void setWarmingUp() noexcept {
            warming_up.store(true, std::memory_order_relaxed);
        }

        inline void resetWarmingUp() noexcept {
            if (warming_up.load(std::memory_order_relaxed)) {
                warming_up.store(false, std::memory_order_relaxed);
            }
		}
    };

    extern LogWatcher watcher;

} 
