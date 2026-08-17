#pragma once

#include <windows.h>

struct SettingsData;

namespace Colors
{
    struct ZoneColors
    {
        COLORREF primaryColor = RGB(170, 205, 255);
        COLORREF borderColor = RGB(255, 255, 255);
        COLORREF highlightColor = RGB(255, 255, 255);
        COLORREF numberColor = RGB(0, 0, 0);
        int highlightOpacity = 50;
    };

    ZoneColors GetZoneColors(const SettingsData& settings) noexcept;
}
