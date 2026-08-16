#pragma once

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace MonitorUtils
{
    using MonitorRect = std::pair<HMONITOR, RECT>;

    // Stable monitor identity (v1): derived from EnumDisplayDevices, no WMI.
    struct Display
    {
        std::wstring deviceId;   // "\\?\DISPLAY#<id>#<instance>#..." interface string
        std::wstring instanceId; // instance component of the interface string
        int number = 0;          // N from "\\.\DISPLAY<N>"
    };

    Display GetDevice(HMONITOR monitor);
    // Stable lookup key for applied-layouts.json (interface string, or the
    // "\\.\DISPLAY<N>" name as a fallback when the interface string is empty).
    std::wstring GetDeviceKey(HMONITOR monitor);

    // Work-area rects (rcWork) of all monitors, in EnumDisplayMonitors order.
    std::vector<MonitorRect> GetAllMonitorWorkRects();

    // Deterministic ordering: left-to-right, top-to-bottom (FancyZones OrderMonitors).
    void OrderMonitors(std::vector<MonitorRect>& monitorInfo);
    std::vector<HMONITOR> GetMonitorsOrdered();

    // Bounding box of all given monitor rects (empty input -> {0,0,0,0}).
    RECT GetMonitorsCombinedRect(const std::vector<MonitorRect>& monitorRects);

    // DPI of a monitor (GetDpiForMonitorInternal, falling back to the primary DC).
    UINT GetDpiForMonitor(HMONITOR monitor) noexcept;

    // Work areas to lay zones out over: one per monitor, or a single combined
    // rect spanning all monitors when span is true. Ordered deterministically.
    std::vector<MonitorRect> GetWorkAreas(bool span);
}
