#pragma once

#include <windows.h>

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

    // Reads the zone colors from the current settings.
    ZoneColors GetZoneColors() noexcept;
}
