
#include "watcher.hpp"
#include "settings.hpp"
#include "aggregator.hpp"

void Logwatch::LogWatcher::releaseNotifications() {

    if (messages.q.empty())
        return;

    const auto& st = Logwatch::GetSettings();

    const auto now = Clock::now();

    if (messages.nextAt.time_since_epoch().count() == 0) {
        messages.nextAt = now + std::chrono::seconds(st.HUDDelaySec);
        return;
    }

	if (now < messages.nextAt) {
		return;
	}

	std::string message = std::move(messages.q.front());
	messages.q.pop_front();

	SKSE::GetTaskInterface()->AddTask(
		[m = std::move(message)]() {
			RE::DebugNotification(m.c_str());
		}
	);

	messages.nextAt = now + std::chrono::seconds(st.HUDDelaySec);
}

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
        entry.type = MailType::PinnedAlert;
        entry.when = std::chrono::system_clock::now();
        entry.title = modKey;
        std::string message = "";

        bool firstPiece = true;
        if (d.errors > 0) {
            message += std::to_string(d.errors) + " new error" + (d.errors > 1 ? "s" : "");
            firstPiece = false;
        }
        if (d.warnings > 0 && minLevel >= 1) {
            if (!firstPiece) message += ", ";
            message += std::to_string(d.warnings) + " new warning" + (d.warnings > 1 ? "s" : "");
            firstPiece = false;
        }
        if (d.fails > 0 && minLevel >= 2) {
            if (!firstPiece) message += ", ";
            message += std::to_string(d.fails) + " new fail" + (d.fails > 1 ? "s" : "");
        }

        entry.summary = message;

        // TODO: make it inline maybe?
        MailModDiff md;
        md.mod = modKey;
        md.errors = d.errors;
        md.warnings = d.warnings;
        md.fails = d.fails;
        entry.mods.push_back(std::move(md));

        scheduleNotification(modKey + ": " + message);
        scheduleMail(std::move(entry));

        updatePinnedBase(modKey, curr, now);
    }
}
