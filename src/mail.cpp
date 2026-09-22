
#include "watcher.hpp"
#include "settings.hpp"
#include "translate.hpp"
#include "utils.hpp"

void Logwatch::LogWatcher::updatePeriodicBase(const ModStatsMap& statsByMod, const Clock::time_point& now, const int& interval) {
    periodicLastPerMod.clear();
    periodicLastTotals = {};
    for (const auto& [modKey, s] : statsByMod) {

        const Counts c = s.counts;

        periodicLastPerMod[modKey] = c;
        periodicLastTotals.errors += c.errors;
        periodicLastTotals.warnings += c.warnings;
        periodicLastTotals.fails += c.fails;
        periodicLastTotals.others += c.others;
    }
    periodicNextAt = now + std::chrono::seconds(interval);
}

void Logwatch::LogWatcher::mayNorifyPeriodicAlerts(const ModStatsMap& statsByMod) {

    const auto st = Logwatch::ReadSettings();

    if (!st.notificationsEnabled || !st.periodicSummaryEnabled) return;

    if (!isGameReady()) return;

    const auto now = Clock::now();

    const int intervalSec = (st.periodicIntervalSec > 0) ? st.periodicIntervalSec : 300;
    const int maxMods = (st.periodicMaxMods > 0) ? st.periodicMaxMods : 5;
    const int minLevel = std::min(st.periodicMinLevel, 2);

    // Compute initial base
    if (!periodicReady) {
        updatePeriodicBase(statsByMod, now, intervalSec);
        periodicReady = true;
        return;
    }

    if (now < periodicNextAt) return;

    // Compute diff since last time
    std::vector<EntryDiff> modDiffs;
    modDiffs.reserve(statsByMod.size());

    Counts totalCounts{};

    for (const auto& [modKey, s] : statsByMod) {

        const Counts curr = s.counts;

        Counts prev{};
        if (auto it = periodicLastPerMod.find(modKey); it != periodicLastPerMod.end())
            prev = it->second;

        Counts d;
        computeDiff(d, curr, prev);

        const uint64_t levelcount = levelCount(d, minLevel);

        if (!levelcount) continue; // nothing to notify

        EntryDiff e;
        e.mod = modKey;
        e.counts = d;
        e.levelCount = levelcount;
        modDiffs.push_back(std::move(e));

        totalCounts.errors += d.errors;
        totalCounts.warnings += d.warnings;
        totalCounts.fails += d.fails;
        totalCounts.others += d.others;
    }

    // Update base
    updatePeriodicBase(statsByMod, now, intervalSec);

    if (modDiffs.empty()) return;

    std::sort(modDiffs.begin(), modDiffs.end(), EntryDiffMore{});

    // Craft message
    const int modsWithIssues = int(modDiffs.size());
    const int modsToShow = std::min(modsWithIssues, maxMods);

    MailEntry entry;
    entry.type = MailType::PeriodicAlert;
    entry.when = std::chrono::system_clock::now();
    entry.title = Trans::Tr("Notify.Periodic.Title");
    Utils::replaceAll(entry.title, "{sec}", std::to_string(intervalSec));

    entry.summary = Trans::Tr("Notify.Periodic.Total");
    Utils::replaceAll(entry.summary, "{errors}", std::to_string(totalCounts.errors));
    Utils::replaceAll(entry.summary, "{warnings}", std::to_string(totalCounts.warnings));
    Utils::replaceAll(entry.summary, "{fails}", std::to_string(totalCounts.fails));
    Utils::replaceAll(entry.summary, "{mods}", std::to_string(modsWithIssues));

    for (auto i = 0; i < modsToShow; ++i) {
        MailModDiff md;
        md.mod = modDiffs[i].mod;
        md.counts = modDiffs[i].counts;
        entry.mods.push_back(std::move(md));
    }

    scheduleMail(std::move(entry));
}
