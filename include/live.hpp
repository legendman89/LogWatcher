#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "SKSEMenuFramework.h"
#include "table.hpp"
#include "window.hpp"
#include "watcher.hpp" 
#include "aggregator.hpp"
#include "notification.hpp"

namespace Live {

    using Clock = std::chrono::steady_clock;

    class LogWatcherUI {

    private:

        static void DrawTable(
            std::vector<TableRow>&  rows,
            const std::vector<int>& view,
            int&                    selected,
            Column&                 sortColumn,
            bool&                   sortAsc);

        static void DrawHUDText(
            ImDrawList*                     drawList,
            const ImVec2&                   screen,
            const Logwatch::HUDMessage&     hud,
            const float&                    alpha,
            const float&                    scale);

        static void RenderDetailsWindow();

    public:

        static void RenderWatch();
        static void RenderMailbox();
        static void RenderSettings();
        static void RenderHUDOverlay();

    };

    void Register();

};