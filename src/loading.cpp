
#include "loading.hpp"
#include "watcher.hpp"


void Live::placeOffset(const float& offset) {
    const float availx = GetAvail().x;
    if (availx > offset) {
        ImGui::SameLine(GetWinMax().x - offset);
	}
}

void Live::placeTopRight(const float& reserve) {
    const float availx = GetAvail().x;
    const auto style = ImGui::GetStyle();
    if (style && availx > reserve) {
        const float offset = GetWinMax().x - style->WindowPadding.x - reserve;
        ImGui::SameLine(offset > 0.0f ? offset : 0.0f, offset > 0.0f ? 0.0f : 50.0f);
    }
    else {
        ImGui::SameLine(0.0f, 50.0f);
	}
}

void Live::renderBusy(const std::string& msg, const float& elapsed, const int& maxDots) {
    float alpha; incLoadingAlpha(alpha, elapsed);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    const double t = ImGui::GetTime();
    const float phase = std::fmod(t, BusyTimings::CYCLE) / BusyTimings::CYCLE;
    const int steps = maxDots * 2;
    int step = int(phase * steps);
    if (step > steps) step = steps;
    const int dots = (step <= maxDots) ? step : (steps - step);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s ", msg.c_str() ? msg.c_str() : "");
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::OFF_TXT);
    ImGui::TextUnformatted(buf);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::Text("%.*s%*s", dots, "...", maxDots - dots, "");
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
}

bool Live::renderDone(BusyState& state, const float& elapsed) {
    float alpha = 1.0;
    decayLoadingAlpha(alpha, elapsed);
    if (alpha <= 0.0f) { state = BusyState::Idle; }
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::SuccessGreen);
    FontAwesome::PushSolid();
    constexpr unsigned FA_CHECK = 0xF00C;
    static auto s = FontAwesome::UnicodeToUtf8(FA_CHECK);
    ImGui::TextUnformatted(s.c_str());
    FontAwesome::Pop();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (state == BusyState::Idle) return true;
	return false;
}

void Live::renderDoneWithLabel(BusyState& state, const std::string& msg, const float& elapsed) {
    float alpha = 1.0;
    decayLoadingAlpha(alpha, elapsed);
    if (alpha <= 0.0f) { state = BusyState::Idle; }
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::OFF_TXT);
    ImGui::TextUnformatted(msg.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::SuccessGreen);
    FontAwesome::PushSolid();
    constexpr unsigned FA_CHECK = 0xF00C;
    static auto s = FontAwesome::UnicodeToUtf8(FA_CHECK);
    ImGui::TextUnformatted(s.c_str());
    FontAwesome::Pop();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}


void Live::renderLoadingOverlay(const std::string& msg, const float& offset, Positioner pos) {

    if (Logwatch::watcher.getRunState() == Logwatch::RunState::Stopped)
		return;

    static BusyContext ctx{};
    const double now = ImGui::GetTime();
    const bool warm = Logwatch::watcher.isWarmingUp();

    if (ctx.state == BusyState::Idle && warm) {
        ctx.state = BusyState::Working;
        ctx.t0 = now;
    }
    else if (ctx.state == BusyState::Working && !warm) {
        const float shown = float(now - ctx.t0);
        if (shown >= BusyTimings::FADE_IN) {
            ctx.state = BusyState::Done;
            ctx.t0 = now;
        }
    }

    // Position
    ImVec2 saved; ImGui::GetCursorScreenPos(&saved);
    if (pos) {
        pos(offset);
    }
    else {
        placeTopRight(BusyTimings::RESERVE);
	}

    const float elapsed = (float)(now - ctx.t0);
	switch (ctx.state) {
		case BusyState::Working:
			renderBusy(msg, elapsed);
			break;
		case BusyState::Done:
            renderDoneWithLabel(ctx.state, msg, elapsed);
			break;
		default:
			break;
	}

    ImVec2 curr; ImGui::GetCursorScreenPos(&curr);
	saved.x = curr.x;
    ImGui::SetCursorScreenPos(saved);
}