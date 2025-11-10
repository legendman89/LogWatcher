#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "SKSEMenuFramework.h"
#include "table.hpp"
#include "window.hpp"
#include "aggregator.hpp" 
#include "watcher.hpp" 

namespace Live {

    class LogWatcherUI {
    public:

        static void RenderWatch();
        static void RenderSettings();

    private:

        static void DrawTable(
            std::vector<TableRow>&  rows,
            const std::vector<int>& view,
            int&                    selected,
            Column&                 sortColumn,
            bool&                   sortAsc);

        static void RenderDetailsWindow();
    };

    void Register();

};