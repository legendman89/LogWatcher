#pragma once

#include <string>
#include <vector>
#include "SKSEMenuFramework.h"

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

    inline DetailsState& Details() {
        static DetailsState s;
        return s;
    }

    inline std::string LevelsPreview(const DetailsState& ds) {
        std::string p;
        if (ds.showError)   p += "Error, ";
        if (ds.showWarning) p += "Warning, ";
        if (ds.showFail)    p += "Fail, ";
        if (ds.showOther)   p += "Other, ";
        if (!p.empty()) p.resize(p.size() - 2);
        if (p.empty())  p = "None";
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

