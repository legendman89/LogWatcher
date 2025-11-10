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
    SKSE::log::debug("Level:{} File:{} Line No.:{} Text:{}", m.level, m.file, m.lineNo, m.line);
    aggr.add(m);
}

Logwatch::LogType Logwatch::classify(const std::filesystem::path& p) {
    const std::string s = p.string();
    return (s.find("Logs\\Script") != std::string::npos ||
        s.find("Logs/Script") != std::string::npos)
        ? LogType::Papyrus
        : LogType::Generic;
}

bool Logwatch::LogWatcher::shouldInclude(const fs::path& file) const {
    const auto s = file.filename().string();
    if (!std::regex_search(s, config.includeFileRegex)) return false;
    if (std::regex_search(s, config.excludeFileRegex)) return false;
    return true;
}

void Logwatch::LogWatcher::discoverFiles(std::vector<fs::path>& out, const fs::path& root, const std::stop_token& stop) {
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    if (!fs::is_directory(root, ec)) return;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator() && !stop.stop_requested(); ++it) {
        if (ec) { ec.clear(); continue; }
        const auto& p = it->path();
        if (fs::is_regular_file(p, ec) && shouldInclude(p)) {
            out.push_back(p);
        }
    }
}

void Logwatch::LogWatcher::workerLoop(const std::stop_token& stop) {

    while (!stop.stop_requested()) {

		// Unlocked scan (only critical parts have locks)
        scanOnce(stop);

        resetWarmingUp();

        // get poll interval without keeping _mutex_ locked
        std::chrono::milliseconds sleep_for;
        {
            std::lock_guard lock(_mutex_);
            sleep_for = config.pollInterval;
        }

        // Stop-aware wait (efficient than just sleeping)
        std::unique_lock wake_lock(_wake_mutex_);
        const auto until = std::chrono::steady_clock::now() + sleep_for;
        _wake_cv_.wait_until( wake_lock, until, [&] { return stop.stop_requested(); } );
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
        const auto canon = fs::weakly_canonical(p, ec).string();

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

    // Cache keys only under lock to avoid stalling I/O
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

void Logwatch::LogWatcher::emitIfMatch(const fs::path& file, std::string_view line, uint64_t lineNo) {
    for (const auto& [name, rx] : config.patterns) {
        if (std::regex_search(line.begin(), line.end(), rx)) {
            if (callback) {
                Match m;
                m.file = Utils::spacify(file.filename().string());
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

void Logwatch::LogWatcher::resetAllFileStates(const bool& fromEnd) {
    std::lock_guard lock(_mutex_);
    logger::info("Resetting all file states to {}", fromEnd ? "end" : "beginning");
    for (auto& [_, file] : files) {
        std::error_code ec;
        auto size = std::filesystem::file_size(file.path, ec);
        if (ec) {
            logger::error("Could not get size for {}: {}", file.path.string(), ec.message());
            size = 0;
        }
        file.state.sizeLastSeen = size;
        file.state.offset = fromEnd ? size : 0;
        file.state.lineNo = 0;
        file.state.lastPoll = Clock::now();
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
    if (ec) { logger::info("exists({}): {}", p.string(), ec.message()); return; }
    const bool is_dir = std::filesystem::is_directory(p, ec);
    if (ec) { logger::info("is_directory({}): {}", p.string(), ec.message()); return; }

    if (exists && is_dir) {
        try {
            watcher.addDirectory(p);
            logger::info("Watching {}", p.string());
        }
        catch (const std::exception& e) {
            logger::error("addDirectory({}) failed: {}", p.string(), e.what());
        }
    }
    else {
        logger::info("Skipping {}", p.string());
    }
}

void Logwatch::LogWatcher::startLogWatcher() {
    try {
        watcher.setWarmingUp();
        watcher.setCallback(OnMatch);
        watcher.start();

        const auto& cfg = watcher.configurator();
        logger::info("Watcher started with configuration:");
        logger::info("  deepScan               : {}", cfg.deepScan ? "true" : "false");
        logger::info("  watchPapyrus           : {}", cfg.watchPapyrus ? "true" : "false");
        logger::info("  pollInterval           : {} ms", cfg.pollIntervalMs);
        logger::info("  cacheCap               : {} messages", cfg.cacheCap);
        logger::info("  maxChunkBytes          : {} KB", cfg.maxChunkKB);
        logger::info("  maxLineBytes           : {} KB", cfg.maxLineKB);
        logger::info("  papyrusMaxChunkBytes   : {} KB", cfg.papyrusMaxChunkKB);
        logger::info("  papyrusMaxLineBytes    : {} KB", cfg.papyrusMaxLineKB);
        logger::info("  autoBoostOnBacklog     : {}", cfg.autoBoostOnBacklog ? "true" : "false");
        logger::info("  backlogThresholdBytes  : {} MB", cfg.backlogBoostThresholdMB);
        logger::info("  backlogBoostFactor     : {:.2f}", cfg.backlogBoostFactor);
        logger::info("  maxBoostCapBytes       : {} MB", cfg.maxBoostCapMB);
        for (size_t i = 0; i < cfg.patterns.size(); ++i) {
            logger::info("  Regex pattern[{}] key  : {}", i, cfg.patterns[i].first);
        }
    }
    catch (const std::exception& e) {
        logger::error("Watcher start failed: {}", e.what());
    }
}