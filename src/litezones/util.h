#pragma once

#include <windows.h>

#include <vector>

// Pure geometry/cycle helpers ported from FancyZonesLib/util.cpp (no WinRT deps).
namespace Util
{
    // Returns the index into zoneRects of the zone reached by moving from the
    // window center in the given arrow direction, or zoneRects.size() when
    // there is no zone in that direction. Mirrors FancyZonesUtils::ChooseNextZoneByPosition.
    size_t ChooseNextZoneByPosition(DWORD vkCode, RECT windowRect, const std::vector<RECT>& zoneRects) noexcept;

    // Offsets windowRect by one full work area opposite to vkCode so the next
    // ChooseNextZoneByPosition call wraps around the opposite edge.
    constexpr RECT PrepareRectForCycling(RECT windowRect, RECT workAreaRect, DWORD vkCode) noexcept
    {
        LONG deltaX = 0;
        LONG deltaY = 0;
        switch (vkCode)
        {
        case VK_UP:
            deltaY = workAreaRect.bottom - workAreaRect.top;
            break;
        case VK_DOWN:
            deltaY = workAreaRect.top - workAreaRect.bottom;
            break;
        case VK_LEFT:
            deltaX = workAreaRect.right - workAreaRect.left;
            break;
        case VK_RIGHT:
            deltaX = workAreaRect.left - workAreaRect.right;
            break;
        default:
            break;
        }

        windowRect.left += deltaX;
        windowRect.right += deltaX;
        windowRect.top += deltaY;
        windowRect.bottom += deltaY;
        return windowRect;
    }
}
