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

void Live::LogWatcherUI::RenderWatch() {

	auto& ps = GetPanel();

	const ImGuiChildFlags childFlags = ImGuiChildFlags_Border;
	int pushes = 0;

	AdjustBorder(pushes);

	// whole panel lives inside section
	bool pushedPanelBg = false;
	if (ps.opaque) {
		SolidBackground(ImGuiCol_ChildBg);
		pushedPanelBg = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1400, 900)); pushes++;
	if (!ImGui::BeginChild("lw_panel", ImVec2(0, 0), childFlags,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {
		if (pushedPanelBg) ImGui::PopStyleColor();
		ImGui::EndChild(); ImGui::PopStyleVar(pushes);
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

	ImGui::EndChild(); ImGui::PopStyleVar(pushes); // lw_panel
	
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
	auto& ds = GetDetails();
	if (!ds.open) return;

	bool pushedWinBg = false;
	if (ds.opaque) {
		SolidBackground(ImGuiCol_WindowBg);
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

void Live::LogWatcherUI::RenderMailbox()
{
	auto entries = Logwatch::watcher.snapshotMailbox();
	static int selected = -1;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1400, 900));
	if (!ImGui::BeginChild("lw_mailbox_panel", ImVec2(0, 0), ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {
		ImGui::EndChild();
		ImGui::PopStyleVar();
		return;
	}

	// Left: mailbox table
	ImGui::BeginChild("lw_mailbox_list", ImVec2(0.6f * GetAvail().x, 0),
		ImGuiChildFlags_Border, ImGuiWindowFlags_None);

	if (ImGui::BeginTable("MailboxTable", 4,
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerH |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableHeadersRow();

		for (auto i = 0; i < (int)entries.size(); ++i) {

			const auto& e = entries[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			// TODO: replace type with fontawesome icons
			const char* typeLabel = "";
			ImVec4 typeColor(1, 1, 1, 1);
			switch (e.type) {
				case Logwatch::MailType::PeriodicAlert:
				{
					typeLabel = "Periodic";
					typeColor = Colors::White;
					break;
				}
				case Logwatch::MailType::PinnedAlert:
				{
					typeLabel = "Pinned";
					typeColor = Colors::PinGold;
					break;
				}
			}
			ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
			ImGui::TextUnformatted(typeLabel);
			ImGui::PopStyleColor();

			// Title
			ImGui::TableNextColumn();
			bool rowSelected = (i == selected);
			if (ImGui::Selectable(e.title.c_str(), rowSelected,
				ImGuiSelectableFlags_SpanAllColumns)) {
				selected = i;
			}

			// Summary
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.summary.c_str());

			// When
			ImGui::TableNextColumn();
			auto when = SinceWhen(e.when);
			ImGui::TextUnformatted(when.c_str());
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// Right: details for selected entry
	ImGui::BeginChild("lw_mailbox_detail", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);

	if (entries.empty()) {
		selected = -1;
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, Colors::DimGray);
		ImGui::TextDisabled("No new messages.");
		ImGui::PopStyleColor();
	}
	else if (selected < 0 || selected >= (int)entries.size()) {
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, Colors::DimGray);
		ImGui::TextDisabled("Select a message on the left to see details.");
		ImGui::PopStyleColor();
	}
	else {
		const auto& e = entries[selected];

		auto when = FormatWhen(e.when);

		ImGui::TextWrapped("%s", e.title.c_str());
		ImGui::Separator();
		ImGui::TextUnformatted(e.summary.c_str());
		ImGui::Text("%s", when.c_str());
		ImGui::Separator();

		if (!e.mods.empty()) {
			if (ImGui::BeginTable("MailboxDetailMods", 4,
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Errors", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("Fails", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableHeadersRow();

				for (const auto& m : e.mods) {
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(m.mod.c_str());

					ImGui::TableNextColumn();
					if (m.errors > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
					ImGui::Text("%llu", m.errors);
					if (m.errors > 0) ImGui::PopStyleColor();

					ImGui::TableNextColumn();
					if (m.errors > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Warning);
					ImGui::Text("%llu", m.warnings);
					if (m.errors > 0) ImGui::PopStyleColor();

					ImGui::TableNextColumn();
					if (m.errors > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Fail);
					ImGui::Text("%llu", m.fails);
					if (m.errors > 0) ImGui::PopStyleColor();
				}

				ImGui::EndTable();
			}
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_TextDisabled, Colors::DimGray);
			ImGui::TextDisabled("No details for this mod.");
			ImGui::PopStyleColor();
		}

		// TODO: a button to jump to Watch
		// if (ImGui::Button("View in Watch")) ...
	}

	ImGui::EndChild();

	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void Live::LogWatcherUI::DrawHUDText(
	ImDrawList*						drawList,
	const ImVec2&					screen,
	const Logwatch::HUDMessage&		hud,
	const float&					alpha,
	const float&					scale)
{
	if (hud.empty() || alpha <= 0.0f)
		return;

	ImVec4 defaultColor = Colors::White;
	defaultColor.w = alpha;

	float totalWidth = 0.0f;
	for (const auto& seg : hud) {
		ImVec2 segSize;
		ImGui::CalcTextSize(&segSize, seg.text.c_str(), nullptr, false, 0.0f);
		totalWidth += segSize.x;
	}

	// Defensive code to avoid dividing by 0 below.
	if (totalWidth <= 0)
		return;

	constexpr float MARGIN_X = 30.0f;
	constexpr float MARGIN_Y = 30.0f;
	constexpr float SCALE_MAX = 2.0f;
	const auto defaultFont = ImGui::GetFont();
	const float availableWidth = screen.x - SCALE_MAX * MARGIN_X;
	
	float scale2fit = 1.0;
	if (availableWidth > 0.0f) {
		scale2fit = availableWidth / totalWidth;
	}

	float fontScale = std::min(scale2fit, scale);
	float scaledWidth = totalWidth * fontScale;
	float scaledSize = defaultFont->FontSize * fontScale;

	ImVec2 pos(screen.x - scaledWidth - MARGIN_X, MARGIN_Y);

	for (const auto& seg : hud) {
		ImVec2 segSize;
		ImGui::CalcTextSize(&segSize, seg.text.c_str(), nullptr, false, 0.0f);
		const auto& segColor = ImGui::ColorConvertFloat4ToU32(LevelColor(seg.level, alpha));
		ImGui::ImDrawListManager::AddText(drawList, defaultFont, scaledSize, pos, segColor, seg.text.c_str());
		pos.x += segSize.x * fontScale;
	}
}


void Live::LogWatcherUI::RenderHUDOverlay()
{
	if (SKSEMenuFramework::IsAnyBlockingWindowOpened())
		return;

	if (!Logwatch::watcher.isGameReady())
		return;

	if (!Logwatch::watcher.isHUDTimeReady())
		return;

	const auto now = Clock::now();

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	ImVec2 screen = ImGui::GetIO()->DisplaySize;

	const auto& st = Logwatch::GetSettings();

	float scale = st.HUDFontScale / 100.0;
	if (scale <= 0.0f) scale = 1.0f;

	static Logwatch::HUDOverlay hudOverlay{};

	// We only fade HUD if it's already on screen.
	if (hudOverlay.active) {
		float age = std::chrono::duration<float>(now - hudOverlay.t0).count();
		const float showmax = float(st.HUDStayOnSec);
		const float fadeout = float(st.HUDFadeOutSec);

		if (age > showmax + fadeout) {
			hudOverlay.active = false;
			hudOverlay.nextAt = now + std::chrono::seconds(st.HUDDelaySec);
			return;
		}

		// Keep drawing but with fading alpha.
		float alpha = 1.0f;
		Logwatch::decayHUDAlpha(alpha, age, showmax, fadeout);
		if (alpha > 0.0f) 
			DrawHUDText(drawList, screen, hudOverlay.current, alpha, scale);
		
		return;
	}

	if (hudOverlay.nextAt.time_since_epoch().count() != 0 && now < hudOverlay.nextAt)
		return;

	if (Logwatch::watcher.isHUDQueueEmpty())
		return;

	Logwatch::watcher.popHUDMessage(hudOverlay.current);
	hudOverlay.t0 = now;
	hudOverlay.active = true;

	// Draw the notification instantly (i.e. alpha = 1).
	DrawHUDText(drawList, screen, hudOverlay.current, 1.0f, scale);
}

void Live::Register() {
	if (!SKSEMenuFramework::IsInstalled()) {
		return;
	}
	SKSEMenuFramework::SetSection("Log Watcher");
	SKSEMenuFramework::AddSectionItem("Watch", LogWatcherUI::RenderWatch);
	SKSEMenuFramework::AddSectionItem("Mailbox", LogWatcherUI::RenderMailbox);
	SKSEMenuFramework::AddSectionItem("Settings", LogWatcherUI::RenderSettings);
	SKSEMenuFramework::AddHudElement(LogWatcherUI::RenderHUDOverlay);
}
