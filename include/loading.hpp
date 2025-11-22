#pragma once

#include "SKSEMenuFramework.h"
#include "helper.hpp"
#include "loading.hpp"
#include "color.hpp"
#include <functional>

namespace Live {

    enum class BusyState { Idle, Working, Done };

    struct BusyTimings {
        static constexpr float RESERVE    = 160.0f;
        static constexpr float FADE_IN    = 0.5f;
        static constexpr float DONE_HOLD  = 0.8f;
        static constexpr float FADE_OUT   = 0.9f;
		static constexpr float CYCLE      = 0.8f;
	};

    struct BusyContext {
        BusyState   state{ BusyState::Idle };
        double      t0{ 0.0 };
	};

    constexpr float CLAMP_ALPHA(float x) noexcept {
        return std::clamp(x, 0.0f, 1.0f);
    }

    using Positioner = std::function<void(const float& reserve)>;

    void placeTopRight(const float& reserve);
    void placeOffset(const float& offset);
    bool renderDone(BusyState& state, const float& elapsed);
    void renderDoneWithLabel(BusyState& state, const std::string& msg, const float& elapsed);
    void renderBusy(const std::string& msg, const float& elapsed, const int& maxDots = 3);
    void renderLoadingOverlay(const std::string& msg, const float& offset, Positioner pos = {});

}
