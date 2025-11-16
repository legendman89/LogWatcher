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

namespace Logwatch {

    enum class RunState { Running, AutoStopPending, Stopped };

    void OnMatch(const Match& m);

    LogType classify(const std::filesystem::path& p);

    using Callback = std::function<void(const Match&)>;

    class LogWatcher {
    private:

		// Run state for auto-stop button
        std::atomic<RunState> runState{ RunState::Running };
        std::atomic<bool> firstPollDone{ false };

		// Warming up overlay state
        std::atomic<bool> warming_up{ true };

        // For locks
        std::mutex _mutex_;

        // Sleep/wakeup for the worker
        std::mutex                  _wake_mutex_;
        std::condition_variable_any _wake_cv_;

        // Worker thread
        std::jthread worker;

        Config   config;
        Callback callback;

        std::vector<fs::path>                         roots;
        std::unordered_map<std::string, FileInfo>     files;

        // Worker body
        void workerLoop(const std::stop_token& stop);

        // Scan functions
        void scanOnce(const std::stop_token& stop);
        bool shouldInclude(const fs::path& file) const;
        void discoverFiles(std::vector<fs::path>& out, const fs::path& root, const std::stop_token& stop);
        void tailFile(FileInfo& fi, const std::stop_token& stop);
        void parseBufferAndEmit(FileInfo& fi, std::string&& chunk, const std::stop_token& stop);
        void emitIfMatch(const fs::path& file, std::string_view line, uint64_t lineNo);

    public:
        LogWatcher() = default;
        ~LogWatcher() { stop(); }

        bool isWarmingUp() const noexcept { return warming_up.load(std::memory_order_relaxed); }

        void startLogWatcher();

        // Directories
        void addLogDirectories();
        void addIfExists(const fs::path& p);

        // Access/modify config
        inline Config& configurator() { return config; }
        inline const Config& configurator() const { return config; }

        // Add a directory (recursive)
        inline void addDirectory(const fs::path& dir) {
            std::lock_guard lock(_mutex_);
            roots.push_back(dir);
        }

        inline void setCallback(Callback cb) {
            std::lock_guard lock(_mutex_);
            callback = std::move(cb);
        }

        // Start/stop background thread
        inline void start() {
            if (worker.joinable()) {
                logger::warn("Watcher thread is already running; start ignored");
                return;
            }
            logger::info("Starting Watcher thread");
            worker = std::jthread(
                [this](const std::stop_token& st) { 
                    try {
                        workerLoop(st);
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
            _wake_cv_.notify_all(); // can wake any sleeping worker
            worker.join();
        }

        inline void setRunState(const RunState& rs) {
            runState.store(rs, std::memory_order_relaxed);
            _wake_cv_.notify_all();
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

        // Clear files map
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
