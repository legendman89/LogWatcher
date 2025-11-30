#include "aggregator.hpp"

Logwatch::Aggregator Logwatch::aggr(500);

void Logwatch::Aggregator::add(const Match& m) {
    const std::string key = keyOfFast(m.file);

    std::unique_lock lock(_mutex_);
	auto& s = mods.try_emplace(key).first->second; // avois creating temporary copies of ModStats

	uint8_t mask = Level::kOther;
	if (m.level == "error") { ++s.errors; mask = Level::kError; }
	else if (m.level == "warning") { ++s.warnings; mask = Level::kWarning; }
	else if (m.level == "fail") { ++s.fails; mask = Level::kFail; }
	else { ++s.others; }

    s.last.emplace_back( ModStats::Record{ m.level, m.file, m.line, m.lineNo, m.when, mask });

    const auto c = cap.load(std::memory_order_relaxed);
    while (s.last.size() > c) s.last.pop_front();
}


std::vector<Logwatch::ModStats::Record>
Logwatch::Aggregator::recent(const std::string& modKey, const size_t& limit) const {
    std::shared_lock lock(_mutex_);
    std::vector<ModStats::Record> out;
    auto it = mods.find(modKey);
    if (it == mods.end() || limit == 0) return out;

    const auto& dq = it->second.last;
    const auto n = std::min(limit, dq.size());
    out.resize(n);
    std::copy_n(dq.rbegin(), n, out.begin());
    return out;
}

std::vector<Logwatch::ModStats::Record>
Logwatch::Aggregator::recentLevel(const std::string& modKey, const size_t& limit, const uint8_t& reqMask) const {
    std::shared_lock lock(_mutex_);
    std::vector<ModStats::Record> out;
    auto it = mods.find(modKey);
    if (it == mods.end() || limit == 0) return out;

    const auto& dq = it->second.last;
    out.reserve(std::min(limit, dq.size()));
    for (auto rit = dq.rbegin(); rit != dq.rend() && out.size() < limit; ++rit) {
        if (rit->levelMask & reqMask) out.push_back(*rit);
    }
    return out;
}
