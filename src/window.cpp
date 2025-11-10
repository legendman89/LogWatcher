
#include "window.hpp"
#include "helper.hpp"
#include "aggregator.hpp"

void Live::multiSelectCombo(DetailsState& ds) {
	std::string preview = LevelsPreview(ds);
	solidBackground(ImGuiCol_PopupBg);
	if (ImGui::BeginCombo("Levels", preview.c_str(), ImGuiComboFlags_WidthFitPreview)) {
		if (ImGui::Checkbox("Error", &ds.showError)) {}
		if (ImGui::Checkbox("Warning", &ds.showWarning)) {}
		if (ImGui::Checkbox("Fail", &ds.showFail)) {}
		if (ImGui::Checkbox("Other", &ds.showOther)) {}
		ImGui::Separator();
		if (ImGui::Button("All")) {
			ds.showError = ds.showWarning = ds.showFail = ds.showOther = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("None")) {
			ds.showError = ds.showWarning = ds.showFail = ds.showOther = false;
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleColor(); // pop solid background
}

void Live::sliderAndAll(DetailsState& ds, const int& recentCached) {
	if (ds.showAll) {
		ds.recentLimit = recentCached;
	}
	else {
		// Ensure slider is within range
		if (ds.recentLimit > recentCached) ds.recentLimit = recentCached;
		if (ds.recentLimit < 1 && recentCached > 0) ds.recentLimit = 1;
	}

	ImGui::BeginGroup(); // slider + All

	ImGui::PushItemWidth(220.0f);
	if (recentCached <= 0) {
		ImGui::BeginDisabled();
		int zero = 0;
		ImGui::SliderInt("Recent", &zero, 0, 0);
		ImGui::EndDisabled();
	}
	else {
		if (ds.showAll) ImGui::BeginDisabled();
		ImGui::SliderInt("Recent", &ds.recentLimit, 1, recentCached);
		if (ds.showAll) ImGui::EndDisabled();
	}
	ImGui::PopItemWidth();

	ImGui::SameLine(0.0f, 12.0f); 	// space before All

	// check box toggled?
	if (ImGui::Checkbox("All", &ds.showAll)) {
		if (ds.showAll) {
			// store current limit so we can restore later
			ds.prevRecentLimit = ds.recentLimit > 0 ? ds.recentLimit : 1;
			// clamp to max for visual sync while All is on
			ds.recentLimit = recentCached;
		}
		else {
			// restore previous manual value
			if (recentCached <= 0) ds.recentLimit = 0;
			else {
				int restored = (ds.prevRecentLimit > 0) ? ds.prevRecentLimit : recentCached;
				ds.recentLimit = std::clamp(restored, 1, recentCached);
			}
		}
	}

	ImGui::EndGroup(); // end slider + all
}

void Live::printMessages(DetailsState& ds) {

	// Request lines per level
	uint8_t request = 0;
	if (ds.showError)   request |= Logwatch::Level::kError;
	if (ds.showWarning) request |= Logwatch::Level::kWarning;
	if (ds.showFail)    request |= Logwatch::Level::kFail;
	if (ds.showOther)   request |= Logwatch::Level::kOther;

	const std::size_t cap = ds.showAll ? SIZE_MAX : (size_t) ds.recentLimit;
	auto msgs = Logwatch::aggr.recentLevel(ds.mod, cap, request);

	ImGui::BeginChild("lw_details_scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	// Auto-scroll to bottom when receiving messages
	const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f;

	for (const auto& m : msgs) {

		if (!LevelEnabled(ds, m.level)) continue; // if disabled in combo

		if (!ds.filter.PassFilter(m.text.c_str())) continue; // if filtered out

		const std::string when = Live::FormatWhen(m.when);

		if (!m.text.empty()) {
			const ImVec4& levelColor = Live::LevelColor(m.level);
			ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
			ImGui::Text("[%s] line %llu: %s", when.c_str(), m.lineNo, m.text.c_str());
			ImGui::PopStyleColor();
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, Colors::DimGray);
			ImGui::Text("[%s] line %llu: <no message text> ", when.c_str(), m.lineNo);
			ImGui::PopStyleColor();
		}

		ImGui::Separator();
	}

	if (ds.autoScroll && (ImGui::GetScrollMaxY() > 0.0f)) {
		if (atBottom) ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}