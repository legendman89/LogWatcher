
#include "watcher.hpp"
#include "settings.hpp"
#include "aggregator.hpp"
#include "notification.hpp"

void Logwatch::LogWatcher::mayNotifyPinnedAlerts(const Snapshot& snap)
{
    const auto& st = Logwatch::GetSettings();

    if (!st.notificationsEnabled || !st.pinnedAlertsEnabled) return;

    if (!isGameReady()) return;

    const auto now = Clock::now();

    const int minLevel = std::min(st.pinnedMinLevel, 2);
    const int minIssues = (st.pinnedMinNewIssues > 0) ? st.pinnedMinNewIssues : 1;
    const int cooldownSec = (st.pinnedAlertCooldownSec > 0) ? st.pinnedAlertCooldownSec : 60;

    for (const auto& [modKey, s] : snap) {

        if (!Logwatch::aggr.isPinned(modKey))
            continue;

        // TODO: override = operator to copy ModStats
        Counts curr;
        curr.errors = s.errors;
        curr.warnings = s.warnings;
        curr.fails = s.fails;
        curr.others = s.others;

        auto it = pinnedState.find(modKey);
        Counts prev{};
        Clock::time_point lastAt{};
        if (it != pinnedState.end()) {
            prev = it->second.counts;
            lastAt = it->second.lastAlertAt;
        }

        Counts d;
        computeDiff(d, curr, prev);

        const uint64_t levelcount = levelCount(d, minLevel);
        if (levelcount < uint64_t(minIssues)) {
            updatePinnedBase(modKey, curr, now);
            continue;
        }

        if (it != pinnedState.end()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastAt).count();
            if (elapsed < cooldownSec) {
                updatePinnedBase(modKey, curr, now);
                continue;
            }
        }

        // Craft message
        MailEntry entry;
        HUDMessage hud;
        std::string segmentText = modKey + ": ";
        hud.push_back( { segmentText , Level::kOther } );
        entry.type = MailType::PinnedAlert;
        entry.when = std::chrono::system_clock::now();
        entry.title = modKey;
        entry.summary = "";

        bool firstPiece = true;
        if (d.errors > 0) {
            segmentText = std::to_string(d.errors) + " error" + (d.errors > 1 ? "s" : "");
            entry.summary += segmentText;
            hud.push_back({ segmentText, Level::kError });
            firstPiece = false;
        }
        if (d.warnings > 0 && minLevel >= 1) {
            if (!firstPiece) {
                segmentText = ", ";
                entry.summary += segmentText;
                hud.push_back({ segmentText, Level::kOther });
            }
            segmentText = std::to_string(d.warnings) + " warning" + (d.warnings > 1 ? "s" : "");
            entry.summary += segmentText;
            hud.push_back({ segmentText, Level::kWarning });
            firstPiece = false;
        }
        if (d.fails > 0 && minLevel >= 2) {
            if (!firstPiece) {
                segmentText = ", ";
                entry.summary += segmentText;
                hud.push_back({ segmentText, Level::kOther });
            }
            segmentText = std::to_string(d.fails) + " fail" + (d.fails > 1 ? "s" : "");
            entry.summary += segmentText;
            hud.push_back({ segmentText, Level::kFail });
        }

        // TODO: make it inline maybe?
        MailModDiff md;
        md.mod = modKey;
        md.errors = d.errors;
        md.warnings = d.warnings;
        md.fails = d.fails;
        entry.mods.push_back(std::move(md));

        scheduleNotification(std::move(hud));
        scheduleMail(std::move(entry));

        updatePinnedBase(modKey, curr, now);
    }
}
