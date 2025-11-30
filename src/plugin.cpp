
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

    Logwatch::Config config{};

    switch (msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
    {
        logger::info("SKSE finished loading; initiating watcher");
		auto& st = Logwatch::GetSettings(); // load defaults first
		Logwatch::settingsPersister.loadState(); // then load persisted settings
        config = Logwatch::watcher.configurator();
        config.loadFromSettings(st);
        Logwatch::aggr.setCapacity(config.cacheCap);
        Logwatch::watcher.checkRunState();
        Logwatch::watcher.addLogDirectories();
        Logwatch::watcher.startLogWatcher();
        break;
    }
    case SKSE::MessagingInterface::kSaveGame: 
    {
		logger::info("Game save detected; saving watcher pinned mods and settings");
        Logwatch::settingsPersister.saveState();
		break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
    {
        Logwatch::watcher.setGameReady(false);
        break;
    }
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
    {
        logger::info("Loading game detected, delaying notifications by {}", config.HUDPostLoadDelaySec);
        Logwatch::watcher.setGameReady(true);
        Logwatch::watcher.setHUDStartDelay(config.HUDPostLoadDelaySec);
        break;
    }
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    setupLog(spdlog::level::info);
    logger::info("Log Watcher Plugin is Loaded");
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
    Live::Register();
    return true;
}
