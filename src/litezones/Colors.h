#pragma once

#include <windows.h>

struct SettingsData;

namespace Colors
{
    struct ZoneColors
    {
        COLORREF primaryColor;
        COLORREF borderColor;
        COLORREF highlightColor;
        COLORREF numberColor;
        int highlightOpacity;
    };

    ZoneColors GetZoneColors(const SettingsData& settings) noexcept;
}
