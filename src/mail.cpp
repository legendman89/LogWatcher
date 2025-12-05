
#include "watcher.hpp"
#include "settings.hpp"
#include "translate.hpp"

void Logwatch::LogWatcher::updatePeriodicBase(const Snapshot& snap, const Clock::time_point& now, const int& interval) {
    periodicLastPerMod.clear();
    periodicLastTotals = {};
    for (const auto& [modKey, s] : snap) {

        // TODO: again = operator
        Counts c;
        c.errors = s.errors;
        c.warnings = s.warnings;
        c.fails = s.fails;
        c.others = s.others;

        periodicLastPerMod[modKey] = c;
        periodicLastTotals.errors += c.errors;
        periodicLastTotals.warnings += c.warnings;
        periodicLastTotals.fails += c.fails;
        periodicLastTotals.others += c.others;
    }
    periodicNextAt = now + std::chrono::seconds(interval);
}

void Logwatch::LogWatcher::mayNorifyPeriodicAlerts(const Snapshot& snap) {

    auto& st = Logwatch::GetSettings();

    if (!st.notificationsEnabled || !st.periodicSummaryEnabled) return;

    if (!isGameReady()) return;

    const auto now = Clock::now();

    const int intervalSec = (st.periodicIntervalSec > 0) ? st.periodicIntervalSec : 300;
    const int maxMods = (st.periodicMaxMods > 0) ? st.periodicMaxMods : 5;
    const int minLevel = std::min(st.periodicMinLevel, 2);

    // Compute initial base
    if (!periodicReady) {
        updatePeriodicBase(snap, now, intervalSec);
        periodicReady = true;
        return;
    }

    if (now < periodicNextAt) return;

    // Compute diff since last time
    std::vector<EntryDiff> modDiffs;
    modDiffs.reserve(snap.size());

    Counts totalCounts{};

    for (const auto& [modKey, s] : snap) {

        Counts curr;
        curr.errors = s.errors;
        curr.warnings = s.warnings;
        curr.fails = s.fails;
        curr.others = s.others;

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
    updatePeriodicBase(snap, now, intervalSec);

    if (modDiffs.empty()) return;

    // Sort mods by disaster
    std::sort(modDiffs.begin(), modDiffs.end(),
        [](const EntryDiff& a, const EntryDiff& b) {
            return a.levelCount > b.levelCount;
        }
    );

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
        md.errors = modDiffs[i].counts.errors;
        md.warnings = modDiffs[i].counts.warnings;
        md.fails = modDiffs[i].counts.fails;
        entry.mods.push_back(std::move(md));
    }

    scheduleMail(std::move(entry));
}
