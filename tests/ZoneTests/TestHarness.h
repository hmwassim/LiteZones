#pragma once

// Shared test fixtures, macros, and helpers for ZoneTests.

#include "../../src/litezones/LayoutEngine.h"
#include "../../src/litezones/LayoutTypes.h"
#include "../../src/litezones/MonitorManager.h"
#include "../../src/litezones/Zone.h"

#include <windows.h>

#include <array>
#include <iostream>
#include <string>
#include <vector>

using namespace LiteZonesTypes;

namespace
{
    const GUID kLayoutGuid = { 0xF762BAD6, 0xDAA1, 0x4997, { 0x94, 0x97, 0xE1, 0x1D, 0xFE, 0xB7, 0x2F, 0x21 } };

    HMONITOR MockMonitor()
    {
        static uintptr_t s_nextMonitor = 0;
        return reinterpret_cast<HMONITOR>(++s_nextMonitor);
    }

    const LayoutData kGridLayoutData{
        kLayoutGuid,
        ZoneSetLayoutType::Grid,
        /*showSpacing=*/true,
        /*spacing=*/17,
        /*zoneCount=*/4,
        /*sensitivityRadius=*/33
    };

    const std::array<RECT, 9> kWorkAreaRects = {
        RECT{ 0, 0, 1024, 768 },
        RECT{ 0, 0, 1280, 720 },
        RECT{ 0, 0, 1280, 800 },
        RECT{ 0, 0, 1280, 1024 },
        RECT{ 0, 0, 1366, 768 },
        RECT{ 0, 0, 1440, 900 },
        RECT{ 0, 0, 1536, 864 },
        RECT{ 0, 0, 1600, 900 },
        RECT{ 0, 0, 1920, 1080 }
    };

    int g_failures = 0;

    void Report(bool ok, const char* file, int line, const std::string& expr)
    {
        if (!ok)
        {
            ++g_failures;
            std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
        }
    }

#define CHECK(expr) Report((expr), __FILE__, __LINE__, #expr)

    void checkRectsEqual(const RECT& expected, const RECT& actual, const char* file, int line)
    {
        Report(expected.left == actual.left && expected.right == actual.right &&
                   expected.top == actual.top && expected.bottom == actual.bottom,
               file, line, "rectangles are equal");
    }

#define CHECK_RECT(a, b) checkRectsEqual((a), (b), __FILE__, __LINE__)

    void compareZones(const Zone& expected, const Zone& actual, const char* file, int line)
    {
        Report(expected.Id() == actual.Id(), file, line, "zone ids are equal");
        checkRectsEqual(expected.GetZoneRect(), actual.GetZoneRect(), file, line);
    }

#define CHECK_ZONE(expected, actual) compareZones((expected), (actual), __FILE__, __LINE__)

    void checkZones(const Layout* layout, size_t expectedCount, RECT rect, const char* file, int line)
    {
        const auto& zones = layout->Zones();
        Report(zones.size() == expectedCount, file, line, "zone count matches");

        int zoneId = 0;
        for (const auto& zone : zones)
        {
            (void)zoneId;
            const auto& zoneRect = zone.second.GetZoneRect();
            Report(zoneRect.left >= 0, file, line, "left border is >= 0");
            Report(zoneRect.top >= 0, file, line, "top border is >= 0");
            Report(zoneRect.left < zoneRect.right, file, line, "rect.left < rect.right");
            Report(zoneRect.top < zoneRect.bottom, file, line, "rect.top < rect.bottom");
            Report(zoneRect.right <= rect.right, file, line, "right border <= monitor work space");
            Report(zoneRect.bottom <= rect.bottom, file, line, "bottom border <= monitor work space");
            ++zoneId;
        }
    }

#define CHECK_ZONES(layout, count, rect) checkZones((layout), (count), (rect), __FILE__, __LINE__)

    bool IsPrimaryDpi96()
    {
        return MonitorUtils::GetDpiForMonitor(MockMonitor()) == 96;
    }

    void RegisterCanvasLayout(const GUID& uuid, const std::vector<CanvasLayoutInfo::Rect>& zoneRects)
    {
        CustomLayoutData data;
        data.name = L"test canvas";
        data.type = CustomLayoutType::Canvas;
        data.canvas.lastWorkAreaWidth = 1920;
        data.canvas.lastWorkAreaHeight = 1080;
        data.canvas.zones = zoneRects;
        SetCustomLayoutData(uuid, data);
    }
}
