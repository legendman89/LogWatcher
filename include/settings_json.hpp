#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include "settings.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Logwatch {

	class SettingPersister {

		std::mutex _mutex_;

		LogWatcherSettings oldSettings{ };
		size_t oldPinsHash{ 0 };

		// Source: Fowler–Noll–Vo hash function (FNV-1a), 64-bit variant
		inline size_t hashPins(const std::unordered_set<std::string>& pins) {
			constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
			constexpr uint64_t FNV_PRIME = 1099511628211ull;
			uint64_t h = FNV_OFFSET;
			for (auto const& s : pins) {
				uint64_t he = FNV_OFFSET;
				for (unsigned char c : s) { he ^= c; he *= FNV_PRIME; }
				h ^= he; h *= FNV_PRIME;
			}
			return size_t(h);
		}

	public:

		inline std::string GetSettingsPath() {
			const auto root = std::filesystem::path(REL::Module::get().filename()).parent_path();
			return (root / "Data" / "SKSE" / "Plugins" / PRODUCT_NAME / PRODUCT_NAME ".json").string();
		}

		inline json to_json_settings(const LogWatcherSettings& s) {

			#define SETTING2JSON(S, D)  {#S, s.S},

			return {
				FOREACH_BOOL_SETTING(SETTING2JSON)
				FOREACH_SIZE_SETTING(SETTING2JSON)
				FOREACH_FLT_SETTING(SETTING2JSON)
			};
		}

		inline void from_json_settings(const json& j, LogWatcherSettings& s) {

			auto get = [&](const char* k, auto& dst) {
				if (j.contains(k)) 
					dst = j.at(k).get<std::decay_t<decltype(dst)>>();
			};

			#define SETTING2GETTER(S, D)  get(#S, s.S);

			FOREACH_BOOL_SETTING(SETTING2GETTER)
			FOREACH_SIZE_SETTING(SETTING2GETTER)
			FOREACH_FLT_SETTING(SETTING2GETTER)
		}

		void saveState();

		void saveStateAsync();

		bool loadState();

	};

	extern SettingPersister settingsPersister;

}
