
#include <cstring>
#include <string>
#include <atomic>
#include <thread>
#include <filesystem>

#include "live.hpp"
#include "plugin.hpp"
#include "logger.hpp"
#include "watcher.hpp"
#include "aggregator.hpp"
#include "settings_json.hpp"

static void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
    {
        logger::info("SKSE finished loading");
		auto& s = Logwatch::Settings(); // load defaults first
		Logwatch::settingsPersister.loadState(); // then load persisted settings
        auto& config = Logwatch::watcher.configurator();
        config.LoadFromSettings(s);
        Logwatch::aggr.setCapacity(config.cacheCap);
        Logwatch::watcher.checkRunState();
        Logwatch::watcher.addLogDirectories();
        Logwatch::watcher.startLogWatcher();
        break;
    }
    case SKSE::MessagingInterface::kSaveGame: 
    {
		logger::info("Game save detected; saving settings");
        Logwatch::settingsPersister.saveState();
		break;
    }
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SetupLog(spdlog::level::info);
    logger::info("Log Watcher Plugin is Loaded");
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
    Live::Register();
    return true;
}
