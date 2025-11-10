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

    void OnMatch(const Match& m);

    LogType classify(const std::filesystem::path& p);

    using Callback = std::function<void(const Match&)>;

    class LogWatcher {
    private:

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
            worker = std::jthread([this](const std::stop_token& st) { workerLoop(st); });
        }

        inline void stop() {
            if (!worker.joinable()) return;
            logger::info("Stopping Watcher thread");
            worker.request_stop();
            _wake_cv_.notify_all(); // can wake any sleeping worker
            worker.join();
        }

        // Reset all file states (offset/line/time).
        void resetAllFileStates(const bool& fromEnd);

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
