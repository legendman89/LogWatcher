
#include <cstring>
#include <string>
#include <atomic>
#include <thread>
#include <filesystem>

#include "live.hpp"
#include "plugin.hpp"
#include "logger.hpp"
#include "watcher.hpp"
#include "translate.hpp"
#include "aggregator.hpp"
#include "settings_json.hpp"


static void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
    {
        logger::info("SKSE finished loading. Starting the watcher.");
		Logwatch::settingsPersister.loadState();
		const auto st = Logwatch::ReadSettings();
        Logwatch::watcher.configurator().loadFromSettings(st);
        const auto& config = Logwatch::watcher.configurator();
        Logwatch::aggr.setCapacity(config.cacheCap);
        Logwatch::watcher.checkRunState();
        Logwatch::watcher.addLogDirectories();
        Logwatch::watcher.startLogWatcher();
        break;
    }
    case SKSE::MessagingInterface::kSaveGame: 
    {
		logger::info("Game saved. Saving settings and pinned mods.");
        Logwatch::settingsPersister.saveState();
		break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
    {
        Logwatch::watcher.setGameReady(false);
        Logwatch::watcher.resetNotifications();
        break;
    }
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
    {
        Logwatch::watcher.resetNotifications();
        const auto delay = Logwatch::ReadSettings().HUDPostLoadDelaySec;
        logger::info("Game loaded. HUD notifications will begin in {} seconds.", delay);
        Logwatch::watcher.setGameReady(true);
        Logwatch::watcher.setHUDStartDelay(delay);
        break;
    }
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    
    setupLog(spdlog::level::info);
    
    SKSE::Init(skse, false);
    
    logger::info("{} v{} by {} (Game v{})", BEAUTIFUL_NAME, CURR_VERSION, AUTHOR_NAME, REL::Module::get().version().string("."));
    
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) {
        logger::critical("Failed to register SKSE message listener");
        return false;
    }

    logger::info("SKSE message listener is registered successfully");

    Trans::GetTranslator().load();

    Live::Register();

    return true;
}
