#pragma once

#include <string>
#include <vector>
#include "SKSEMenuFramework.h"
#include "translate.hpp"

using namespace ImGuiMCP;

namespace Live {

    struct DetailsState {
        std::string     mod;
        ImGuiTextFilter filter;
        int             recentLimit = 500;
        int             prevRecentLimit = -1;
        bool            open = false;
        bool            autoScroll = true;
        bool            opaque = false;
        bool            showError = true;
        bool            showWarning = true;
        bool            showFail = true;
        bool            showOther = true;
        bool            showAll = false;
    };

    inline DetailsState& GetDetails() {
        static DetailsState s;
        return s;
    }

    inline std::string LevelsPreview(const DetailsState& ds) {
        std::string p;
        if (ds.showError)   p += Trans::Tr("Watch.Table.Header.Errors"), p += ", ";
        if (ds.showWarning) p += Trans::Tr("Watch.Table.Header.Warnings"), p += ", ";
        if (ds.showFail)    p += Trans::Tr("Watch.Table.Header.Fails"), p += ", ";
        if (ds.showOther)   p += Trans::Tr("Watch.Table.Header.Others"), p += ", ";
        if (p.empty())
            p = Trans::Tr("Watch.Details.Combo.None");
        else 
            p.resize(p.size() - 2);
        return p;
    }

    inline bool LevelEnabled(const DetailsState& ds, const std::string& lvl) {
        if (lvl == "error")   return ds.showError;
        if (lvl == "warning") return ds.showWarning;
        if (lvl == "fail")    return ds.showFail;
        return ds.showOther;
    }

    void multiSelectCombo(DetailsState& ds);

    void sliderAndAll(DetailsState& ds, const int& recentCached);

    void printMessages(DetailsState& ds);

}

