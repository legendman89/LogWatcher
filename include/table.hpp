#pragma once

#include <string>
#include <vector>
#include "SKSEMenuFramework.h"
#include "color.hpp"
#include "helper.hpp"
#include "aggregator.hpp"

namespace Live {

#define FOREACH_COLUMN(COL) \
    COL(Mod,      ImGuiTableColumnFlags_None,                   110.0f) \
    COL(Errors,   ImGuiTableColumnFlags_DefaultSort |                   \
                  ImGuiTableColumnFlags_PreferSortDescending,   45.0f) \
    COL(Warnings, ImGuiTableColumnFlags_PreferSortDescending,   45.0f) \
    COL(Fails,    ImGuiTableColumnFlags_PreferSortDescending,   45.0f) \
    COL(Others,   ImGuiTableColumnFlags_PreferSortDescending,   45.0f) \
    COL(Recent,   ImGuiTableColumnFlags_PreferSortDescending,   45.0f) \
    COL(Pinned,   ImGuiTableColumnFlags_None,                   35.0f)


#define COL2SETUP(NAME, FLAGS, WIDTH) ImGui::TableSetupColumn(#NAME, FLAGS, WIDTH);
#define COL2ENUM(NAME, FLAGS, WIDTH) NAME,
#define COL2STR(NAME, FLAGS, WIDTH) #NAME,

    enum class Column : int { FOREACH_COLUMN(COL2ENUM) Count };

    static const char* COLSTR[] = { FOREACH_COLUMN(COL2STR) };

    struct PanelState {
        ImGuiTextFilter filter;
        int             recentLimit = 50;
        int             selected = -1;     // index in filtered view
        Column          sortColumn = Column::Errors;
        bool            sortAsc = false;  // desc
        bool            pinFirst = true;
        bool            showPinnedOnly = false;
        bool            opaque = false;
    };

    struct TableRow {
        std::string mod;
        int errors{};
        int warnings{};
        int fails{};
        int others{};
        int recent{};
        bool pinned{};
    };

    inline PanelState& GetPanel() {
        static PanelState s;
        return s;
    }

    inline void PinStyle(TableRow& r) {
        constexpr unsigned STAR = 0xF005;
        static const std::string starText = FontAwesome::UnicodeToUtf8(STAR);
        if (r.pinned) {
            FontAwesome::PushSolid();
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::PinGold);
        }
        else {
            FontAwesome::PushRegular();
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::PinMuted);
		}
        NoButtonBorder(true);
        bool clicked = ImGui::SmallButton((starText + "##pin").c_str());
        NoButtonBorder(false);
        ImGui::PopStyleColor();
        FontAwesome::Pop();
        if (clicked) {
            Logwatch::aggr.setPinned(r.mod, !r.pinned);
            r.pinned = !r.pinned;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(r.pinned ? "Unpin" : "Pin");
    }

    void addTableControls(PanelState& ps);

    void takeSnapshot(std::vector<TableRow>& rows);

    void filterTable(PanelState& ps, std::vector<int>& view, const std::vector<TableRow>& rows);

    void sortTable(PanelState& ps, std::vector<int>& view, const std::vector<TableRow>& rows);

    void buildTable(int& selected, std::vector<TableRow>& rows, const std::vector<int>& view);

}
