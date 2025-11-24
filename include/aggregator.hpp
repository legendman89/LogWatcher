#pragma once

#include <shared_mutex>
#include <unordered_set>
#include <filesystem>

#include "statistics.hpp"
#include "logger.hpp"
#include "state.hpp"

namespace Logwatch {

	class Aggregator {

	private:

		// For bookkeeping; backup is needed when we switch to deep scan.
		Snapshot mods, backup;
		std::unordered_set<std::string> pinned;

		// To remind myself, I added mutable to allow unlocking in constant functions.
		mutable std::shared_mutex _mutex_;

		// Cache capacity.
		std::atomic_size_t cap;

		// This version is faster than the standard stem.
		inline std::string keyOfFast(const std::string_view& path) {
			const auto sep = path.find_last_of("/\\");
			auto fname = (sep == std::string_view::npos) ? path : path.substr(sep + 1);
			const auto dot = fname.find_last_of('.');
			auto stem = (dot == std::string_view::npos) ? fname : fname.substr(0, dot);
			return std::string(stem);
		}

	public:

		explicit Aggregator(const size_t& maxMsgs = 500) : cap(maxMsgs) {
			logger::info("Aggregator initialized with capacity {}", cap.load(std::memory_order_relaxed));
			mods.reserve(128);
		}

		void add(const Match& m);

		std::vector<ModStats::Record> recent(const std::string& modKey, const size_t& limit = SIZE_MAX) const;
		std::vector<ModStats::Record> recentLevel(const std::string& modKey, const size_t& limit, const uint8_t& levelMask) const;

		// Deep Scan ON
		inline void backupAndClear() {
			std::unique_lock lock(_mutex_);
			backup = std::move(mods);
			mods.clear();
		}

		// Deep Scan OFF
		inline void restoreAndClear() {
			std::unique_lock lock(_mutex_);
			if (!backup.empty()) {
				mods = std::move(backup);
				backup.clear();
				const auto c = cap.load(std::memory_order_relaxed);
				for (auto& [_, s] : mods) while (s.last.size() > c) s.last.pop_front();
			}
			else {
				mods.clear();
			}
		}

		inline void invalidateBackup() {
			std::unique_lock lock(_mutex_);
			backup.clear();
		}

		inline void clear() {
			std::unique_lock lock(_mutex_);
			mods.clear();
		}

		inline void clearPins() {
			std::unique_lock lock(_mutex_);
			pinned.clear();
		}

		inline Snapshot snapshot() const {
			std::shared_lock lock(_mutex_);
			Snapshot out;
			out.reserve(mods.size()); // avoids rehashing
			for (const auto& m : mods) out.emplace(m.first, m.second);
			return out;
		}

		inline std::unordered_set<std::string> snapshotPins() const {
			std::shared_lock lock(_mutex_);
			std::unordered_set<std::string> pins;
			pins.reserve(pinned.size()); // avoids rehashing
			pins.insert(pinned.begin(), pinned.end());
			return pins;
		}

		inline void reset(const std::string& modKey) {
			std::unique_lock lock(_mutex_);
			mods.erase(modKey);
		}

		inline void setCapacity(const size_t& n) {
			std::unique_lock lock(_mutex_);
			cap.store(n, std::memory_order_relaxed);
			for (auto& [_, s] : mods) {
				while (s.last.size() > n) s.last.pop_front();
			}
			logger::info("Aggregator capacity set to {}", n);
		}

		// TODO: do we need to nudge threads here?
		inline size_t capacity() const {
			return cap.load(std::memory_order_relaxed);
		}


		inline void replacePins(const std::unordered_set<std::string>& pins) {
			std::unique_lock lock(_mutex_);
			pinned.clear();
			pinned.insert(pins.begin(), pins.end());
		}

		inline bool isPinned(const std::string& mod) const {
			std::shared_lock lock(_mutex_);
			return pinned.count(mod) != 0;
		}

		inline void setPinned(const std::string& mod, const bool& pin) {
			std::unique_lock lock(_mutex_);
			if (pin) pinned.insert(mod); 
			else pinned.erase(mod);
		}


	};

	extern Aggregator aggr;

}
