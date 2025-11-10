#pragma once

#include "SKSEMenuFramework.h"

namespace Colors {

    // Neutrals
    inline constexpr ImVec4 White = { 0.90f, 0.90f, 0.90f, 1.00f };
    inline constexpr ImVec4 DimGray = { 0.65f, 0.65f, 0.70f, 1.00f };
    inline constexpr ImVec4 SteelHeaderBG = { 0.15f, 0.15f, 0.20f, 0.90f };
    inline constexpr ImVec4 BlueGrayHeaderTxt = { 0.85f, 0.88f, 0.92f, 1.00f };

    // Level counts
    inline constexpr ImVec4 Error = { 0.93f, 0.26f, 0.26f, 1.00f };
    inline constexpr ImVec4 Warning = { 0.98f, 0.80f, 0.20f, 1.00f };
    inline constexpr ImVec4 Fail = { 0.70f, 0.35f, 0.90f, 1.00f };
    inline constexpr ImVec4 Other = { 0.40f, 0.85f, 0.90f, 1.00f };

    // Pins
    inline constexpr ImVec4 PinGold = { 1.00f, 0.75f, 0.20f, 1.00f };
    inline constexpr ImVec4 PinMuted = { 0.65f, 0.65f, 0.70f, 1.00f };

    // Selection
    inline constexpr ImVec4 RowHover = { 0.25f, 0.25f, 0.35f, 0.80f };
    inline constexpr ImVec4 RowActive = { 0.30f, 0.30f, 0.45f, 0.90f };

    // Indicators
    inline constexpr ImVec4 SpinnerGray = { 0.70f, 0.70f, 0.70f, 1.00f };
    inline constexpr ImVec4 SuccessGreen = { 0.24f, 0.78f, 0.35f, 1.00f };

    // Opaque variants
    inline constexpr ImVec4 WindowBG_Opaque = { 0.10f, 0.10f, 0.12f, 1.00f };
    inline constexpr ImVec4 ChildBG_Opaque = { 0.12f, 0.12f, 0.14f, 1.00f };
    inline constexpr ImVec4 PopupBG_Opaque = { 0.10f, 0.10f, 0.12f, 1.00f };

	// Text button colors
    inline constexpr ImVec4 OFF_TXT = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
}
