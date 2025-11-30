#include <algorithm>
#include <codecvt>
#include <locale>
#include <filesystem>

#include "config.hpp"
#include "watcher.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "documents.hpp"
#include "aggregator.hpp"

Logwatch::LogWatcher Logwatch::watcher;

void Logwatch::OnMatch(const Match& m) {
    if (m.file.find("Log Watcher") == std::string::npos) {
        SKSE::log::debug("File:{} Level:{} Line No.:{}", m.file, m.level, m.lineNo);
    }
    aggr.add(m);
}

Logwatch::LogType Logwatch::classify(const std::filesystem::path& p) {
    const std::string s = Utils::toUTF8(p);
    return (s.find("Logs\\Script") != std::string::npos || s.find("Logs/Script") != std::string::npos)
        ? LogType::Papyrus : LogType::Generic;
}

bool Logwatch::LogWatcher::shouldInclude(const fs::path& file) const {
    const auto s = Utils::toUTF8(file.filename());
    if (!std::regex_search(s, config.includeFileRegex)) return false;
    if (std::regex_search(s, config.excludeFileRegex)) return false;
    return true;
}

void Logwatch::LogWatcher::discoverFiles(std::vector<fs::path>& out, const fs::path& root, const std::stop_token& stop) {

    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;

    auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    if (ec) {
        const std::string rootToPrint = Utils::replaceUsername(Utils::toUTF8(root));
        logger::info("discoverFiles: cannot iterate over {}: {}", rootToPrint, ec.message());
        return;
    }

    while (it != end) {

        if (stop.stop_requested()) break;

        const auto& p = it->path();
        
        // collect files
        if (fs::is_regular_file(p, ec) && shouldInclude(p)) {
            out.push_back(p);
        }

        ec.clear();

        it.increment(ec); // increment iterator safely

        if (ec) {
            const std::string rootToPrint = Utils::replaceUsername(Utils::toUTF8(root));
            logger::info("discoverFiles: skipping path under {}: {}", rootToPrint, ec.message());
            ec.clear();
        }
    }
}

void Logwatch::LogWatcher::watcherLoop(const std::stop_token& stop) {

    while (!stop.stop_requested()) {

        if (!isFirstPollDone()) {
            logger::info("Watcher initially starting or resumed");
        }

		// Handle paused state
        if (getRunState() == RunState::Stopped) {
            logger::info("Watcher paused; sleeping until resumed");
            std::unique_lock wake_lock(_wake_mutex_);
            _wake_cv_.wait(
                wake_lock,
                [&] {
                    return stop.stop_requested() || getRunState() != RunState::Stopped;
                }
            );
            continue;
        }

        scanOnce(stop); // Unlocked scan (only critical parts have locks)

        resetWarmingUp();

		markFirstPollDone();

        // Schedule notifications / mails
        if (!stop.stop_requested()) {
            const auto snap = aggr.snapshot();
            mayNotifyPinnedAlerts(snap);
            mayNorifyPeriodicAlerts(snap);
        }

		// Handle auto-stop after first poll
        if (getRunState() == RunState::AutoStopPending) {
            logger::info("Watcher pausing after first poll");
			setRunState(RunState::Stopped);
            continue;
		}

        // get poll interval without keeping _mutex_ locked
        std::chrono::milliseconds sleep_for;
        {
            std::lock_guard lock(_mutex_);
            sleep_for = config.pollInterval;
        }

		// Stop-aware and paused-aware sleep
        std::unique_lock wake_lock(_wake_mutex_);
        const auto until = Clock::now() + sleep_for;
        _wake_cv_.wait_until( wake_lock, until, 
            [&] { 
                return stop.stop_requested() || getRunState() == RunState::Stopped;
            } 
        );
    }

    logger::info("Watcher thread exited");
}

void Logwatch::LogWatcher::scanOnce(const std::stop_token& stop) {

    std::vector<fs::path> discovered;
    discovered.reserve(64);
    for (const auto& r : roots) {
        if (stop.stop_requested()) return;
        discoverFiles(discovered, r, stop);
    }

    for (const auto& p : discovered) {
        if (stop.stop_requested()) return;

        std::error_code ec;
        const auto canonPath = fs::weakly_canonical(p, ec);
        const auto canon = Utils::toUTF8(canonPath);

		// critical section: check if we need to insert
        bool need_insert = false;
        bool start_from_end = false;
        {
            std::lock_guard lock(_mutex_);
            need_insert = (files.find(canon) == files.end());
            start_from_end = !config.deepScan;
        }

        if (!need_insert) continue;

		// Prepare FileInfo outside lock
        FileInfo fi;
        fi.path = p;
        fi.type = classify(fi.path);

        fi.state.writeTime = fs::last_write_time(p, ec);
        fi.state.sizeLastSeen = fs::file_size(p, ec);
        fi.state.lastPoll = Clock::now();

        if (ec) ec.clear();
        std::error_code ec2;
        const auto sz = fs::file_size(p, ec2);
        fi.state.sizeLastSeen = ec2 ? 0 : sz;
        fi.state.offset = start_from_end ? fi.state.sizeLastSeen : 0;
        fi.state.lineNo = 0;

		// critical section: insert file if still not present
        {
            std::lock_guard lock(_mutex_);
            if (files.find(canon) == files.end()) {
                files.emplace(canon, std::move(fi));
            }
        }
    }

    if (stop.stop_requested()) return;

    // Cache keys under lock
    std::vector<std::string> keys;
    {
        std::lock_guard lock(_mutex_);
        keys.reserve(files.size());
        for (auto& f : files) keys.push_back(f.first);
    }

	// Work unlocked through the cached keys
    for (const auto& key : keys) {
        if (stop.stop_requested()) return;

		// get state snapshot under lock
        FileInfo snap;
        {
            std::lock_guard lock(_mutex_);
            auto it = files.find(key);
            if (it == files.end()) continue;
			snap = it->second; 
        }

        // I/O phase (unlocked)
        std::error_code ec;
        const bool exists = fs::exists(snap.path, ec);
        const auto size = exists ? fs::file_size(snap.path, ec) : 0ull;
        const auto wt = exists ? fs::last_write_time(snap.path, ec) : decltype(snap.state.writeTime){};

        // Erase locked if the file vanished
        if (!exists) {
            std::lock_guard lock(_mutex_);
            auto it = files.find(key);
            if (it != files.end()) files.erase(it);
            continue;
        }

        // Handle truncation/rotation in the snapshot
        if (size < snap.state.offset) {
            snap.state.offset = 0;
            snap.state.lineNo = 0;
        }

        // Tail if there is new data
        bool tailed = false;
        if (size > snap.state.offset) {
            tailFile(snap, stop);
            tailed = true;
        }

		// Commit updated state back under lock
        {
            std::lock_guard lock(_mutex_);
            auto fit = files.find(key);
            if (fit == files.end()) continue;

            auto& fi = fit->second;
            if (!fs::exists(fi.path, ec)) { files.erase(fit); continue; }

            fi.state.sizeLastSeen = size;
            fi.state.writeTime = wt;
            if (tailed) {
                fi.state.offset = snap.state.offset;
                fi.state.lineNo = snap.state.lineNo;
            }
        }
    }
}


void Logwatch::LogWatcher::tailFile(FileInfo& fi, const std::stop_token& stop) {
    std::error_code ec;
    const auto size = fs::file_size(fi.path, ec);
    if (ec || size <= fi.state.offset) return;

    size_t chunkCap = KB2B(fi.type == LogType::Papyrus ? config.papyrusMaxChunkKB : config.maxChunkKB);

    if (config.autoBoostOnBacklog) {
        const uint64_t backlog = (size > fi.state.offset) ? (size - fi.state.offset) : 0;
        if (backlog > MB2B(config.backlogBoostThresholdMB)) {
            double boosted = double(chunkCap) * config.backlogBoostFactor;
            chunkCap = size_t(std::min<double>(boosted, double(MB2B(config.maxBoostCapMB))));
        }
    }

    const auto toRead = std::min<uint64_t>(chunkCap, size - fi.state.offset);

    if (stop.stop_requested()) return;

    std::ifstream in(fi.path, std::ios::binary);
    if (!in) return;

    in.seekg(static_cast<std::streamoff>(fi.state.offset), std::ios::beg);
    
    std::string buf;
    buf.resize(toRead);
    in.read(&buf[0], static_cast<std::streamsize>(toRead));

    const auto offset = static_cast<size_t>(in.gcount());
    buf.resize(offset);
    fi.state.offset += offset;

    parseBufferAndEmit(fi, std::move(buf), stop);
}

void Logwatch::LogWatcher::parseBufferAndEmit(FileInfo& fi, std::string&& chunk, const std::stop_token& stop) {
    
    const size_t lineCap = KB2B(fi.type == LogType::Papyrus ? config.papyrusMaxLineKB : config.maxLineKB);

    size_t start = 0;
    while (start < chunk.size() && !stop.stop_requested()) {
        size_t end = chunk.find_first_of("\r\n", start);
        if (end == std::string::npos) end = chunk.size();

        const size_t len = end - start;
        if (len <= lineCap) {
            // This must be string_view to avoid allocating enw memory for chunk.
            std::string_view sv(&chunk[start], len);
            auto line = Utils::trimLine(sv);
            if (!line.empty()) {
                ++fi.state.lineNo;
                emitIfMatch(fi.path, line, fi.state.lineNo);
            }
        }

        // eat newline chars
        if (end < chunk.size()) {
            if (chunk[end] == '\r' && end + 1 < chunk.size() && chunk[end + 1] == '\n')
                start = end + 2;
            else
                start = end + 1;
        }
        else {
            start = end;
        }
    }
}

void Logwatch::LogWatcher::emitIfMatch(const fs::path& file, const std::string_view& line, const uint64_t& lineNo) {
    for (const auto& [name, rx] : config.patterns) {
        if (std::regex_search(line.begin(), line.end(), rx)) {
            if (callback) {
                Match m;
                const auto fileName = Utils::toUTF8(file.filename());
                m.file = Utils::spacify(fileName);
                m.line = Utils::nukeLogLine(std::string(line));
                m.keyword = name;
                if (name == "error")        m.level = "error";
                else if (name == "warning") m.level = "warning";
                else if (name == "fail")    m.level = "fail";
                else                        m.level = "other";
                m.lineNo = lineNo;
                m.when = std::chrono::system_clock::now();
                callback(m);
            }
            return; // stop after first matching pattern
        }
    }
}

void Logwatch::LogWatcher::addLogDirectories() {
    const auto gameRoot = std::filesystem::path(REL::Module::get().filename()).parent_path();
    addIfExists(gameRoot / "Data" / "SKSE" / "Plugins");

    const auto docs = GetDocumentsDir();
    if (!docs.empty()) {
        addIfExists(docs / "My Games" / "Skyrim Special Edition" / "SKSE");
        addIfExists(docs / "My Games" / "Skyrim.INI" / "SKSE");
        addIfExists(docs / "My Games" / "Skyrim Special Edition" / "SKSE" / "Plugins");
        addIfExists(docs / "My Games" / "Skyrim" / "SKSE");
        addIfExists(docs / "My Games" / "Skyrim" / "SKSE" / "Plugins");

        // Papyrus logs
        if (config.watchPapyrus) addIfExists(docs / "My Games" / "Skyrim Special Edition" / "Logs" / "Script");
    }
    else {
        logger::error("Documents folder not found");
    }
}

void Logwatch::LogWatcher::addIfExists(const std::filesystem::path& p) {

    std::error_code ec;
    const bool exists = std::filesystem::exists(p, ec);

    const auto pathToPrint = Utils::replaceUsername(Utils::toUTF8(p));

    if (ec) {
        logger::info("exists({}): {}", pathToPrint, ec.message());
        return; 
    }

    const bool is_dir = std::filesystem::is_directory(p, ec);

    if (ec) { 
        logger::info("is_directory({}): {}", pathToPrint, ec.message());
        return; 
    }

    if (exists && is_dir) {
        try {
            addDirectory(p);
            logger::info("Watching {}", pathToPrint);
        }
        catch (const std::exception& e) {
            logger::error("addDirectory({}) failed: {}", pathToPrint, e.what());
        }
    }
    else {
        logger::info("Skipping {}", pathToPrint);
    }
}

void Logwatch::LogWatcher::startLogWatcher() {
    try {
        resetNotifications();

        setWarmingUp();
        setCallback(OnMatch);
        start();

        logger::info("Watcher started with configuration:");
        config.print();
    }
    catch (const std::exception& e) {
        logger::error("Watcher start failed: {}", e.what());
    }
}