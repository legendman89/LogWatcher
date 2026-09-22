#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "state.hpp"
#include "statistics.hpp"

// TODO: change this to HUD
namespace Logwatch {

    struct PinnedAlertState {
        Clock::time_point lastAlertAt{};

        Counts counts;
    };

    struct HUDSegment {
        std::string text;
        Level level;
    };

    typedef std::vector<HUDSegment> HUDMessage;

    struct HUDOverlay {
        HUDMessage current;
        Clock::time_point t0{};
        Clock::time_point nextAt{};
        Clock::time_point pausedAt{};
        uint64_t notificationGeneration{ 0 };
        bool active{ false };
        bool paused{ false };
    };

    inline void decayHUDAlpha(float& alpha, const float& age, const float& STAY_ON, const float& FADE_OUT) {
        if (age > STAY_ON) {
            float d = (age - STAY_ON) / FADE_OUT;
            if (d > 1.0f) d = 1.0f;
            alpha = 1.0f - d;
        }
    }

} 
