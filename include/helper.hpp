#pragma once

#include <string>
#include "SKSEMenuFramework.h"
#include "color.hpp"

namespace Live {

    inline ImVec2 GetAvail() {
        ImVec2 out{};
        ImGui::GetContentRegionAvail(&out);
        return out;
    }

    inline ImVec2 GetWinMax() {
        ImVec2 out{};
        ImGui::GetWindowContentRegionMax(&out);
        return out;
    }

    inline const ImVec4& LevelColor(const std::string& lvl) {
        if (lvl == "error") return Colors::Error;
        if (lvl == "warning") return Colors::Warning;
        if (lvl == "fail") return Colors::Fail;
        return Colors::Other;
    }

    inline void HelpMarker(const char* text) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", text);
    }

    inline std::string FormatWhen(const std::chrono::system_clock::time_point& tp) {
        using namespace std::chrono;
        auto t = system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf);
    }

    inline void solidBackground(const ImGuiCol& bgType) {
        auto bg = ImGui::GetStyleColorVec4(bgType);
        ImVec4 newBg = *bg;
        newBg.w = 1.0f; // force opaque
        ImGui::PushStyleColor(bgType, newBg);
    }

    inline void adjustBorder(int& pushes) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);  pushes++;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6)); pushes++;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2)); pushes++;
    }

    inline void noButtonBorder(const bool& push) {
        if (push) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        }
        else {
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }
    }

	inline bool CTAButton(const char* label, const bool& enabled) {

		// On state colors
		ImVec4 onBG = ImVec4(0.95f, 0.96f, 0.98f, 1.00f); // whitish
		ImVec4 onHV = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // hover brighter
		ImVec4 onAC = ImVec4(0.92f, 0.93f, 0.96f, 1.00f); // whitishhh
		ImVec4 onTxt = ImVec4(0.10f, 0.12f, 0.16f, 1.00f); // dark text

		// Off state colors
		ImVec4 offBG = ImVec4(0.35f, 0.37f, 0.40f, 0.55f); 
		ImVec4 offHV = ImVec4(0.40f, 0.42f, 0.46f, 0.60f);
		ImVec4 offAC = ImVec4(0.32f, 0.34f, 0.38f, 0.55f);
		
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.f, 6.f));

		const ImVec4& bg = enabled ? onBG : offBG;
		const ImVec4& hv = enabled ? onHV : offHV;
		const ImVec4& ac = enabled ? onAC : offAC;
		const ImVec4& tx = enabled ? onTxt : Colors::OFF_TXT;

		ImGui::PushStyleColor(ImGuiCol_Button, bg);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hv);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ac);
		ImGui::PushStyleColor(ImGuiCol_Text, tx);

		if (!enabled) ImGui::BeginDisabled();
		bool clicked = ImGui::Button(label);
		if (!enabled) ImGui::EndDisabled();

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
		return clicked && enabled;
	}

    inline void TextUnformattedU8(const char8_t* s) {
        const auto len = std::char_traits<char8_t>::length(s);
        const char* b = reinterpret_cast<const char*>(s);
        ImGui::TextUnformatted(b, b + len);
    }

}

