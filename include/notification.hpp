#pragma once

#include <chrono>
#include <deque>

// TODO: change this to HUD
namespace Logwatch {

    // TODO: move this to statistics to avoid maintaining duplicate code.
    struct Counts {
        uint64_t errors = 0;
        uint64_t warnings = 0;
        uint64_t fails = 0;
        uint64_t others = 0;
    };

    struct PinnedSnapshot {
        Counts counts;
        Clock::time_point lastAlertAt{};
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
        bool active{ false };
    };

    inline void decayHUDAlpha(float& alpha, const float& age, const float& STAY_ON, const float& FADE_OUT) {
        if (age > STAY_ON) {
            float d = (age - STAY_ON) / FADE_OUT;
            if (d > 1.0f) d = 1.0f;
            alpha = 1.0f - d;
        }
    }

} 
