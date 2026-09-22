
#include "table.hpp"
#include "window.hpp"
#include "helper.hpp"
#include "loading.hpp"
#include "aggregator.hpp"
#include "watcher.hpp"
#include "translate.hpp"

bool Live::TableRowSorter::operator()(const int leftIndex, const int rightIndex) const {

	const auto& left = rows[leftIndex];
	const auto& right = rows[rightIndex];

	if (panel.pinFirst && left.pinned != right.pinned) return left.pinned;

	int result = 0;
	switch (panel.sortColumn) {
		case Column::Mod:      result = left.mod < right.mod ? -1 : (left.mod == right.mod ? 0 : 1); break;
		case Column::Errors:   result = compareCount(left.counts.errors, right.counts.errors); break;
		case Column::Warnings: result = compareCount(left.counts.warnings, right.counts.warnings); break;
		case Column::Fails:    result = compareCount(left.counts.fails, right.counts.fails); break;
		case Column::Others:   result = compareCount(left.counts.others, right.counts.others); break;
		case Column::Recent:   result = compareCount(left.recent, right.recent); break;
		case Column::Pinned:   result = compareCount(left.pinned, right.pinned); break;
		case Column::Reset:    result = 0; break;
		case Column::Count:    default: result = 0; break;
	}

	return panel.sortAsc ? result < 0 : result > 0;
}

bool Live::ConfirmReset(const std::string& mod) {
	bool reset = false;
	if (ImGui::BeginPopup("##confirm_reset")) {
		ImGui::TextUnformatted(Trans::Tr("Watch.Table.Reset.Confirm").c_str());
		ImGui::TextColored(Colors::BlueGrayHeaderTxt, "%s", mod.c_str());
		ImGui::Separator();
		if (ImGui::Button(Trans::Tr("Watch.Table.Reset.Button").c_str())) {
			Logwatch::watcher.resetMod(mod);
			reset = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(Trans::Tr("Watch.Table.Reset.Cancel").c_str())) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	return reset;
}

bool Live::ResetButton(const std::string& mod) {
	if (ImGui::Button(Trans::Tr("Watch.Table.Reset.Button").c_str())) ImGui::OpenPopup("##confirm_reset");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(Trans::Tr("Watch.Table.Tooltip.Reset").c_str());
	return ConfirmReset(mod);
}

void Live::ResetStyle(TableRow& r) {
	constexpr unsigned ARROW_ROTATE_LEFT = 0xF0E2;
	static const std::string resetText = FontAwesome::UnicodeToUtf8(ARROW_ROTATE_LEFT);
	FontAwesome::PushSolid();
	ImGui::PushStyleColor(ImGuiCol_Text, Colors::DimGray);
	NoButtonBorder(true);
	const bool clicked = ImGui::SmallButton((resetText + "##reset").c_str());
	NoButtonBorder(false);
	ImGui::PopStyleColor();
	FontAwesome::Pop();
	if (clicked) ImGui::OpenPopup("##confirm_reset");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(Trans::Tr("Watch.Table.Tooltip.Reset").c_str());

	if (!ConfirmReset(r.mod)) return;
	r.counts = {};
	r.recent = 0;
}

void Live::addTableControls(PanelState& ps) {
	ImGui::PushItemWidth(280.0f);
	ps.filter.Draw(Trans::Tr("Watch.Panel.Filter.Label").c_str());
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Checkbox(Trans::Tr("Watch.Panel.Opaque.Label").c_str(), &ps.opaque);

	ImGui::Dummy(ImVec2(0, 5));
	if (ImGui::Button(Trans::Tr("Watch.Panel.ClearPins.Label").c_str())) {
		Logwatch::aggr.clearPins();
	}
	ImGui::SameLine();
	ImGui::Checkbox(Trans::Tr("Watch.Panel.PinFirst.Label").c_str(), &ps.pinFirst);
	ImGui::SameLine();
	ImGui::Checkbox(Trans::Tr("Watch.Panel.ShowPinnedOnly.Label").c_str(), &ps.showPinnedOnly);

	renderLoadingOverlay(Trans::Tr("Watch.Panel.WarmingUp.Label").c_str(), BusyTimings::RESERVE);

	ImGui::Dummy(ImVec2(0, 6));
}

void Live::filterTable(PanelState& ps, std::vector<int>& view, const std::vector<TableRow>& rows) {
	for (int i = 0; i < rows.size(); ++i) {
		if (!ps.filter.PassFilter(rows[i].mod.c_str())) continue;
		if (ps.showPinnedOnly && !rows[i].pinned) continue;
		view.push_back(i);
	}
}

void Live::sortTable(PanelState& ps, std::vector<int>& view, const std::vector<TableRow>& rows) {
	std::stable_sort(view.begin(), view.end(), TableRowSorter(ps, rows));
}

void Live::buildTable(int& selected, std::vector<TableRow>& rows, const std::vector<int>& view) {

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.5f));

	for (int idxInView = 0; idxInView < view.size(); ++idxInView) {

		const int idx = view[idxInView];
		TableRow& r = rows[idx];

		ImGui::TableNextRow();

		// Mod (selectable)
		ImGui::TableNextColumn();
		const bool isSel = (selected == idxInView);
		ImGui::PushID(r.mod.c_str());
		ImGui::PushStyleColor(ImGuiCol_Text, r.pinned ? Colors::PinGold : Colors::White);
		const bool clickedMod = ImGui::Selectable(r.mod.c_str(), isSel);
		ImGui::PopStyleColor();
		ImGui::PopID();
		if (clickedMod) {
			selected = idxInView;
			auto& ds = GetDetails();
			ds.mod = r.mod;
			ds.open = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(Trans::Tr("Watch.Table.Tooltip.ModDetails").c_str());

		// Errors
		ImGui::TableNextColumn();
		if (r.counts.errors > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
		ImGui::Text("%d", r.counts.errors);
		if (r.counts.errors > 0) ImGui::PopStyleColor();

		// Warnings
		ImGui::TableNextColumn();
		if (r.counts.warnings > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Warning);
		ImGui::Text("%d", r.counts.warnings);
		if (r.counts.warnings > 0) ImGui::PopStyleColor();

		// Fails
		ImGui::TableNextColumn();
		if (r.counts.fails > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Fail);
		ImGui::Text("%d", r.counts.fails);
		if (r.counts.fails > 0) ImGui::PopStyleColor();

		// Other
		ImGui::TableNextColumn();
		if (r.counts.others > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Other);
		ImGui::Text("%d", r.counts.others);
		if (r.counts.others > 0) ImGui::PopStyleColor();

		// Recent
		ImGui::TableNextColumn();
		ImGui::Text("%d", r.recent);

		// Pinned
		ImGui::TableNextColumn();
		ImGui::PushID(r.mod.c_str());
		PinStyle(r);
		ImGui::PopID();

		// Reset
		ImGui::TableNextColumn();
		ImGui::PushID(r.mod.c_str());
		ResetStyle(r);
		ImGui::PopID();

	}

	ImGui::PopStyleVar();  // FramePadding
}
