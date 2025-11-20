
#include "logger.hpp"
#include "aggregator.hpp"
#include "settings_json.hpp"

Logwatch::SettingPersister Logwatch::settingsPersister{};

void Logwatch::SettingPersister::saveStateAsync() {
	std::jthread([this]() mutable {
			saveState();
		}
	).detach();
}

void Logwatch::SettingPersister::saveState() {
	std::lock_guard lock(_mutex_);

	const auto path = settingsPath();
	const auto tmp = path + ".tmp";

	try {
		const auto& s = Settings();

		const auto pins = aggr.snapshotPins();
		const size_t pinsHash = hashPins(pins);

		if (s == oldSettings && pinsHash == oldPinsHash) {
			logger::info("Settings and Pins unchanged; skipping saveState");
			return;
		}

		json root;

		root["version"] = 1;
		root["settings"] = to_json_settings(s);

		if (s.persistPins) {
			root["pins"] = pins;
		}

		fs::create_directories(fs::path(path).parent_path());
		{
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			out << root.dump(2);
			if (!out.good()) throw std::runtime_error("write failed");
			out.flush();
		} // ensure file is closed

		std::error_code ec;
		fs::rename(tmp, path, ec); // does atomic replace
		if (ec) throw std::runtime_error("atomic replace failed when saving settings");

		oldSettings = s;
		oldPinsHash = pinsHash;

		logger::info("Saved settings and pins to {}", path);
	}
	catch (const std::exception& e) {
		std::error_code ec;
		fs::remove(tmp, ec);
		logger::error("saveState failed: {}", e.what());
	}
}

bool Logwatch::SettingPersister::loadState() {
	try {
		const auto path = settingsPath();
		if (!fs::exists(path)) { 
			logger::info("No config at {}; using defaults", path); 
			return false;
		}

		std::ifstream in(path, std::ios::binary);
		json root = json::parse(in);

		int ver = root.value("version", 1);
		(void)ver; // (prevents the annoying compiler warnings for now)

		auto& s = Settings();
		if (root.contains("settings")) from_json_settings(root["settings"], s);

		oldSettings = s;

		if (s.persistPins && root.contains("pins") && root["pins"].is_array()) {
			const auto pins = root["pins"].get<std::unordered_set<std::string>>();
			aggr.replacePins(pins);
			oldPinsHash = hashPins(pins);
		}
		else {
			oldPinsHash = 0;
		}

		logger::info("Loaded settings and pins from {}", path);
		return true;
	}
	catch (const std::exception& e) {
		logger::error("loadState failed: {}", e.what());
		return false;
	}
}