#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "live.hpp"
#include "utils.hpp"
#include "helper.hpp"
#include "settings.hpp"
#include "restart.hpp"
#include "loading.hpp"

void Live::takeSnapshot(std::vector<TableRow>& rows) {
	const auto snapMods = Logwatch::aggr.snapshot(); 
	const auto snapPins = Logwatch::aggr.snapshotPins();
	rows.reserve(snapMods.size());
	for (auto& [modKey, s] : snapMods) {
		TableRow r;
		r.mod = modKey;
		r.errors = s.errors;
		r.warnings = s.warnings;
		r.fails = s.fails;
		r.others = s.others;
		r.recent = (int)s.last.size();
		r.pinned = snapPins.count(modKey) != 0;
		rows.push_back(std::move(r));
	}
}

void Live::LogWatcherUI::RenderSettings()
{
	auto& st = Logwatch::Settings();

	static bool settings_init = false;
	static Logwatch::LogWatcherSettings prev_settings{};
	static const Logwatch::LogWatcherSettings factory{};
	if (!settings_init) { prev_settings = st; settings_init = true; }

	solidBackground(ImGuiCol_ChildBg);
	if (ImGui::BeginChild("lw_settings", ImVec2(0, 0), ImGuiChildFlags_None)) {

		if (ImGui::CollapsingHeader("History", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Deep scan (parse from beginning on next start)", &st.deepScan);
			Live::HelpMarker("When enabled, the watcher parses each tracked log from offset 0 on next (re)start. "
				"Otherwise, it tails from the current file end.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Cache", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt("Cache capacity per mod", &st.cacheCap, 100, 20000);
			HelpMarker("How many recent messages per mod to keep in memory. "
				"This bounds what 'All' can show in the Details view.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Performance", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::SliderInt("Poll interval (ms)", &st.pollIntervalMs, 100, 5000);
			HelpMarker("How often to check log files for new data. "
				"Lower values increase responsiveness but use more CPU.");
			ImGui::SliderInt("Max chunk per poll (KB)", &st.maxChunkKB, 16, (1 << 13));
			HelpMarker("Maximum data read from each log file per poll. "
				"Larger chunks reduce I/O calls but may cause hitches if too large.");
			ImGui::SliderInt("Max line length (KB)", &st.maxLineKB, 1, 1024);
			HelpMarker("Lines longer than this are skipped. "
				"Some logs may have very long lines; adjust as needed.");
			ImGui::Dummy(ImVec2(0, 4));

			ImGui::Checkbox("Watch Papyrus logs", &st.watchPapyrus);
			HelpMarker("Enable monitoring of Papyrus script logs. "
				"These logs can be large and may impact performance.");
			ImGui::BeginDisabled(!st.watchPapyrus);
			ImGui::SliderInt("Max Papyrus chunk per poll (KB)", &st.papyrusMaxChunkKB, 256, (1 << 14));
			HelpMarker("Papyrus logs can be large; larger chunks help keep up.");
			ImGui::SliderInt("Max Papyrus line length (KB)", &st.papyrusMaxLineKB, 64, 2048);
			HelpMarker("Maximum line length for Papyrus logs.");
			ImGui::EndDisabled();
			ImGui::Dummy(ImVec2(0, 2));
			ImGui::TextDisabled("Papyrus logs often exceed 1 MB; larger chunks prevent lag.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("Adaptive Boost (advanced)", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Enable auto-boost when behind", &st.autoBoostOnBacklog);
			HelpMarker("When the watcher falls behind (e.g. large log writes), "
				"it can temporarily increase chunk size to catch up faster.");
			ImGui::SliderInt("Backlog threshold (MB)", reinterpret_cast<int*>(&st.backlogBoostThresholdMB), 1, 256);
			HelpMarker("If the unread backlog exceeds this size, boosting activates.");
			ImGui::SliderFloat("Boost factor", &st.backlogBoostFactor, 1.0f, 4.0f, "%.1fx");
			HelpMarker("Multiplier for chunk size during boosting.");
			ImGui::SliderInt("Boost cap (MB)", reinterpret_cast<int*>(&st.maxBoostCapMB), 1, 32);
			HelpMarker("Maximum chunk size during boosting.");
			ImGui::Dummy(ImVec2(0, 2));
			ImGui::TextDisabled("When backlog exceeds threshold, chunk size grows temporarily to catch up.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		if (ImGui::CollapsingHeader("UX (optional)", 0)) {
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::Checkbox("Persist pins to disk", &st.persistPins);
			HelpMarker("When enabled, pinned mods are saved to disk and restored on restart.");
			ImGui::Dummy(ImVec2(0, 4));
		}

		ImGui::Separator();

		// Applying state
		static BusyState busyState = BusyState::Idle;
		const bool inflight = Logwatch::Restart::ApplyOnTheFly();
		const bool finished = Logwatch::Restart::ApplyDone();
		const bool canApply =	 (st != prev_settings)	&& busyState != BusyState::Working;
		const bool canDefaults = (st != factory)		&& busyState != BusyState::Working;

		if (Live::CTAButton("Save and Apply", canApply)) {
			busyState = BusyState::Working;
			Logwatch::ApplyNow();
		}
		ImGui::SameLine(0.0f, 6.0f);
		if (Live::CTAButton("Load Defaults", canDefaults)) {
			busyState = BusyState::Working;
			Logwatch::LoadDefaults(factory);
			Logwatch::ApplyNow();
		}

		// status widget on same line
		ImGui::SameLine(0.0f, 20.0f);
		
		static BusyContext ui_ctx{};
		const double now = ImGui::GetTime();

		// Transition to Working state
		if (ui_ctx.state == BusyState::Idle && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		// If a new apply occurs during Done fade, restart animation
		if (ui_ctx.state == BusyState::Done && busyState == BusyState::Working) {
			ui_ctx.state = BusyState::Working;
			ui_ctx.t0 = now;
		}

		// Transition to Done state
		if (ui_ctx.state == BusyState::Working && !inflight && finished) {
			prev_settings = st;
			ui_ctx.state = BusyState::Done;
			busyState = BusyState::Idle;
			ui_ctx.t0 = now;
		}

		// Render according to state
		const float elapsed = float(now - ui_ctx.t0);
		switch (ui_ctx.state) {
			case BusyState::Working: {
				Live::RenderBusy("Applying", elapsed);
				break;
			}
			case BusyState::Done: {
				const bool faded = Live::RenderDone(ui_ctx.state, elapsed);
				if (faded) busyState = BusyState::Idle;
				break;
			}
			default:
				break;
		}

	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}


void Live::LogWatcherUI::RenderWatch() {

	auto& ps = Panel();

	// whole panel lives inside section
	bool pushedPanelBg = false;
	if (ps.opaque) {
		solidBackground(ImGuiCol_ChildBg);
		pushedPanelBg = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1400, 900));
	if (!ImGui::BeginChild("lw_panel", ImVec2(0, 0), ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {
		if (pushedPanelBg) ImGui::PopStyleColor();
		ImGui::EndChild();
		return;
	}

	// controls
	addTableControls(ps);

	// get aggregator snapshot
	std::vector<TableRow> rows;
	takeSnapshot(rows);
	
	// new view
	std::vector<int> view;
	view.reserve(rows.size());

	// filter view
	filterTable(ps, view, rows);

	// sort view
	sortTable(ps, view, rows);

	// guard selection against filter
	if (ps.selected < 0 || ps.selected >= (int)view.size()) 
		ps.selected = -1;

	// table takes entire panel
	ImGui::BeginChild("lw_table_host", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
	DrawTable(rows, view, ps.selected, ps.sortColumn, ps.sortAsc);
	ImGui::EndChild();

	ImGui::EndChild(); ImGui::PopStyleVar(); // lw_panel
	
	if (pushedPanelBg) ImGui::PopStyleColor();

	RenderDetailsWindow();
}

void Live::LogWatcherUI::DrawTable(
	std::vector<TableRow>& rows,
	const std::vector<int>& view,
	int& selected,
	Column& sortColumn,
	bool& sortAsc) {

	ImGuiTableFlags flags = ImGuiTableFlags_BordersH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Reorderable;


	if (ImGui::BeginTable("lw_table", (int)Column::Count, flags)) {

		ImGui::TableSetupScrollFreeze(0, 1);

		// Header
		FOREACH_COLUMN(COL2SETUP);

		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, Colors::SteelHeaderBG);
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::BlueGrayHeaderTxt);
		ImGui::TableHeadersRow();
		ImGui::PopStyleColor(2);

		if (ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs()) {
			if (sort->SpecsDirty && sort->SpecsCount == 1) {
				int idx = sort->Specs[0].ColumnIndex;
				if (idx < 0 || idx >= (int)Column::Count) idx = (int)Column::Mod;
				sortColumn = static_cast<Column>(idx);
				sortAsc = (sort->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
				sort->SpecsDirty = false;
			}
		}

		// Contents
		buildTable(selected, rows, view);

		ImGui::EndTable();
	}
}

void Live::LogWatcherUI::RenderDetailsWindow() {
	auto& ds = Details();
	if (!ds.open) return;

	bool pushedWinBg = false;
	if (ds.opaque) {
		solidBackground(ImGuiCol_WindowBg);
		pushedWinBg = true;
	}

	ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mod Details", &ds.open, ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::End();
		if (pushedWinBg) ImGui::PopStyleColor();
		return;
	}

	const auto modName = ds.mod;

	if (modName.empty()) {
		ImGui::TextDisabled("No mod selected.");
		ImGui::End();
		if (pushedWinBg) ImGui::PopStyleColor();
		return;
	}

	// Header
	const bool isPinned = Logwatch::aggr.isPinned(modName);
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, isPinned ? Colors::PinGold : Colors::BlueGrayHeaderTxt);
	ImGui::SetWindowFontScale(1.08f);
	ImGui::TextUnformatted(modName.c_str());
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor(1);
	ImGui::SameLine(0.0f, 12.0f);
	ImGui::Checkbox("Auto-scroll", &ds.autoScroll);
	ImGui::SameLine(0.0f, 12.0f);
	ImGui::Checkbox("Opaque", &ds.opaque);
	ImGui::SameLine(0.0f, 12.0f);

	// Multi-select for levels
	multiSelectCombo(ds);

	ImGui::Dummy(ImVec2(0, 5));

	ds.filter.Draw("Filter");

	ImGui::Separator();

	ImGui::Dummy(ImVec2(0, 5));

	// Snapshot summary + recent cached
	const auto snap = Logwatch::aggr.snapshot();
	const auto it = snap.find(modName);
	if (it == snap.end()) {
		ImGui::TextDisabled("No data for this mod yet (maybe log rotated).");
		ImGui::End();
		if (pushedWinBg) ImGui::PopStyleColor();
		return;
	}

	const auto& modstats = it->second;
	const int recentCached = int(modstats.last.size());
	const auto cap = Logwatch::aggr.capacity();

	sliderAndAll(ds, recentCached);

	ImGui::SameLine(0.0f, 18.0f);
	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(Colors::Error, "Errors: %d", modstats.errors);
	ImGui::SameLine(0.0f, 10.0f);
	ImGui::TextColored(Colors::Warning, "Warnings: %d", modstats.warnings);
	ImGui::SameLine(0.0f, 10.0f);
	ImGui::TextColored(Colors::Fail, "Fails: %d", modstats.fails);
	ImGui::SameLine(0.0f, 10.0f);
	ImGui::TextColored(Colors::Other, "Others: %d", modstats.others);
	ImGui::SameLine(0.0f, 12.0f);
	ImGui::TextColored(Colors::DimGray, "(cached: %d / %d)", recentCached, int(cap));

	ImGui::Separator();

	ImGui::Dummy(ImVec2(0, 5));

	printMessages(ds);
	
	ImGui::End(); // end window

	if (pushedWinBg) ImGui::PopStyleColor();
}


void Live::Register() {
	if (!SKSEMenuFramework::IsInstalled()) {
		return;
	}
	SKSEMenuFramework::SetSection("Log Watcher");
	SKSEMenuFramework::AddSectionItem("Watch", LogWatcherUI::RenderWatch);
	SKSEMenuFramework::AddSectionItem("Settings", LogWatcherUI::RenderSettings);
}
