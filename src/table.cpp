
#include "table.hpp"
#include "window.hpp"
#include "helper.hpp"
#include "loading.hpp"
#include "aggregator.hpp"

void Live::addTableControls(PanelState& ps) {
	ImGui::PushItemWidth(280.0f);
	ps.filter.Draw("Filter");
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Checkbox("Opaque", &ps.opaque);

	ImGui::Dummy(ImVec2(0, 5));
	if (ImGui::Button("Clear Pins")) {
		Logwatch::aggr.clearPins();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Pin first", &ps.pinFirst);
	ImGui::SameLine();
	ImGui::Checkbox("Show pinned only", &ps.showPinnedOnly);

	renderLoadingOverlay("Warming up", BusyTimings::RESERVE);

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
	auto sorter = [&](int a, int b) {
		const auto& A = rows[a];
		const auto& B = rows[b];
		if (ps.pinFirst && A.pinned != B.pinned) return A.pinned;
		int res = 0;
		switch (ps.sortColumn) {
		case Column::Mod:      res = (A.mod < B.mod) ? -1 : (A.mod == B.mod ? 0 : 1); break;
		case Column::Errors:   res = (A.errors < B.errors) ? -1 : (A.errors > B.errors ? 1 : 0); break;
		case Column::Warnings: res = (A.warnings < B.warnings) ? -1 : (A.warnings > B.warnings ? 1 : 0); break;
		case Column::Fails:    res = (A.fails < B.fails) ? -1 : (A.fails > B.fails ? 1 : 0); break;
		case Column::Others:   res = (A.others < B.others) ? -1 : (A.others > B.others ? 1 : 0); break;
		case Column::Recent:   res = (A.recent < B.recent) ? -1 : (A.recent > B.recent ? 1 : 0); break;
		case Column::Pinned:   res = (A.pinned < B.pinned) ? -1 : (A.pinned > B.pinned ? 1 : 0); break;
		case Column::Count:    default: res = 0; break;
		}
		return ps.sortAsc ? (res < 0) : (res > 0);
		};

	std::stable_sort(view.begin(), view.end(), sorter);
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
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to view details");

		// Errors
		ImGui::TableNextColumn();
		if (r.errors > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
		ImGui::Text("%d", r.errors);
		if (r.errors > 0) ImGui::PopStyleColor();

		// Warnings
		ImGui::TableNextColumn();
		if (r.warnings > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Warning);
		ImGui::Text("%d", r.warnings);
		if (r.warnings > 0) ImGui::PopStyleColor();

		// Fails
		ImGui::TableNextColumn();
		if (r.fails > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Fail);
		ImGui::Text("%d", r.fails);
		if (r.fails > 0) ImGui::PopStyleColor();

		// Other
		ImGui::TableNextColumn();
		if (r.others > 0) ImGui::PushStyleColor(ImGuiCol_Text, Colors::Other);
		ImGui::Text("%d", r.others);
		if (r.others > 0) ImGui::PopStyleColor();

		// Recent
		ImGui::TableNextColumn();
		ImGui::Text("%d", r.recent);

		// Pinned
		ImGui::TableNextColumn();
		ImGui::PushID(r.mod.c_str());
		PinStyle(r);
		ImGui::PopID();

	}

	ImGui::PopStyleVar();  // FramePadding
}