// Zone/Layout geometry tests ported from PowerToys FancyZonesTests
// (Zone.Spec.cpp, Layout.Spec.cpp), plus a tiny assert harness. No third-party
// test framework. Exit code 0 = all passed.

#include "../../src/litezones/AppZoneHistory.h"
#include "../../src/litezones/json.h"
#include "../../src/litezones/LayoutEngine.h"
#include "../../src/litezones/MonitorManager.h"
#include "../../src/litezones/Paths.h"
#include "../../src/litezones/Settings.h"
#include "../../src/litezones/util.h"
#include "../../src/litezones/WindowProperties.h"
#include "../../src/litezones/Zone.h"

#include <windows.h>

#include <array>
#include <iostream>
#include <memory>
#include <string>

using namespace FancyZonesDataTypes;

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

    void checkZones(const Layout* layout, ZoneSetLayoutType type, size_t expectedCount, RECT rect, const char* file, int line)
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
            if (type != ZoneSetLayoutType::Focus)
            {
                Report(zoneRect.right <= rect.right, file, line, "right border <= monitor work space");
                Report(zoneRect.bottom <= rect.bottom, file, line, "bottom border <= monitor work space");
            }
            ++zoneId;
        }
    }

#define CHECK_ZONES(layout, type, count, rect) checkZones((layout), (type), (count), (rect), __FILE__, __LINE__)

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

void TestZone()
{
    const RECT zoneRect{ 10, 10, 200, 200 };
    Zone zone(zoneRect, 1);
    CHECK(zone.IsValid());
    CHECK_RECT(zoneRect, zone.GetZoneRect());

    const RECT zeroRect{ 0, 0, 0, 0 };
    Zone zeroZone(zeroRect, 1);
    CHECK(zeroZone.IsValid());
    CHECK_RECT(zeroRect, zeroZone.GetZoneRect());

    constexpr ZoneIndex zoneId = 123;
    Zone idZone(zoneRect, zoneId);
    CHECK(idZone.IsValid());
    CHECK(idZone.Id() == zoneId);

    Zone invalidId(zoneRect, -1);
    CHECK(!invalidId.IsValid());

    Zone invalidRect({ 100, 100, 99, 101 }, 1);
    CHECK(!invalidRect.IsValid());
}

void TestLayoutBasics()
{
    auto layout = std::make_unique<Layout>(kGridLayoutData);
    CHECK(layout->Id() == kLayoutGuid);
    CHECK(layout->Type() == ZoneSetLayoutType::Grid);

    CHECK(layout->Zones().empty());
    CHECK(layout->ZonesFromPoint(POINT{ 0, 0 }).empty());
}

void TestZoneFromPoint()
{
    LayoutData data = kGridLayoutData;
    data.spacing = 0;
    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    CHECK(layout->ZonesFromPoint(POINT{ 1, 1 }).size() == 1);
    CHECK(layout->ZonesFromPoint(POINT{ 0, 0 }).size() == 1);
    CHECK(layout->ZonesFromPoint(POINT{ 1920, 1080 }).empty());

    auto spaced = std::make_unique<Layout>(kGridLayoutData);
    spaced->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());
    CHECK(spaced->ZonesFromPoint(POINT{ 1921, 1080 }).empty());
}

void TestZoneFromPointCustom()
{
    // These exercise the custom canvas path with DPI conversion; at 96 DPI the
    // conversion is identity, so expected rects are exact. Skip otherwise.
    if (!IsPrimaryDpi96())
    {
        std::cout << "  [skipped custom-layout cases: primary DPI != 96]\n";
        return;
    }

    const GUID uuid = kLayoutGuid;
    RegisterCanvasLayout(uuid, {
        CanvasLayoutInfo::Rect{ 0, 0, 100, 100 },
        CanvasLayoutInfo::Rect{ 10, 10, 80, 80 },
        CanvasLayoutInfo::Rect{ 10, 10, 140, 140 },
        CanvasLayoutInfo::Rect{ 10, 10, 40, 40 },
    });

    LayoutData data = kGridLayoutData;
    data.type = ZoneSetLayoutType::Custom;
    data.zoneCount = 4;

    Settings::instance().data.overlappingZonesAlgorithm = L"smallest";

    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    auto zones = layout->ZonesFromPoint(POINT{ 50, 50 });
    CHECK(zones.size() == 1);
    Zone expected({ 10, 10, 50, 50 }, 3);
    CHECK_ZONE(expected, layout->Zones().at(zones[0]));
}

void TestZoneFromPointMultizone()
{
    if (!IsPrimaryDpi96())
    {
        std::cout << "  [skipped multizone case: primary DPI != 96]\n";
        return;
    }

    const GUID uuid = kLayoutGuid;
    RegisterCanvasLayout(uuid, {
        CanvasLayoutInfo::Rect{ 0, 0, 100, 100 },
        CanvasLayoutInfo::Rect{ 100, 0, 100, 100 },
        CanvasLayoutInfo::Rect{ 0, 100, 100, 100 },
        CanvasLayoutInfo::Rect{ 100, 100, 100, 100 },
    });

    LayoutData data = kGridLayoutData;
    data.type = ZoneSetLayoutType::Custom;
    data.zoneCount = 4;

    Settings::instance().data.overlappingZonesAlgorithm = L"smallest";

    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    auto actual = layout->ZonesFromPoint(POINT{ 50, 100 });
    CHECK(actual.size() == 2);

    Zone zone1({ 0, 0, 100, 100 }, 0);
    Zone zone3({ 0, 100, 100, 200 }, 2);
    CHECK_ZONE(zone1, layout->Zones().at(actual[0]));
    CHECK_ZONE(zone3, layout->Zones().at(actual[1]));
}

void TestLayoutInitValidValues()
{
    const int zoneCount = 10;

    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.spacing = 10;
        data.zoneCount = zoneCount;

        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            const bool result = layout->Init(rect, MockMonitor());
            CHECK(result);
            CHECK_ZONES(layout.get(), data.type, static_cast<size_t>(zoneCount), rect);
        }
    }
}

void TestLayoutInitInvalidMonitorInfo()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.spacing = 10;
        data.zoneCount = 10;

        auto layout = std::make_unique<Layout>(data);
        CHECK(!layout->Init(RECT{ 0, 0, 0, 0 }, MockMonitor()));
    }
}

void TestLayoutInitZeroSpacing()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.spacing = 0;
        data.zoneCount = 10;

        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            const bool result = layout->Init(rect, MockMonitor());
            CHECK(result);
            CHECK_ZONES(layout.get(), data.type, static_cast<size_t>(data.zoneCount), rect);
        }
    }
}

void TestLayoutInitLargeNegativeSpacing()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.zoneCount = 10;
        data.spacing = ZoneConstants::MAX_NEGATIVE_SPACING - 1;

        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            const bool result = layout->Init(rect, MockMonitor());
            if (type == static_cast<int>(ZoneSetLayoutType::Focus))
            {
                CHECK(result);
            }
            else
            {
                CHECK(!result);
            }
        }
    }
}

void TestLayoutInitHorizontallyBigSpacing()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.zoneCount = 10;

        for (const auto& rect : kWorkAreaRects)
        {
            data.spacing = rect.right;
            auto layout = std::make_unique<Layout>(data);

            const bool result = layout->Init(rect, MockMonitor());
            if (type == static_cast<int>(ZoneSetLayoutType::Focus))
            {
                CHECK(result);
            }
            else
            {
                CHECK(!result);
            }
        }
    }
}

void TestLayoutInitVerticallyBigSpacing()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.zoneCount = 10;

        for (const auto& rect : kWorkAreaRects)
        {
            data.spacing = rect.bottom;
            auto layout = std::make_unique<Layout>(data);

            const bool result = layout->Init(rect, MockMonitor());
            if (type == static_cast<int>(ZoneSetLayoutType::Focus))
            {
                CHECK(result);
            }
            else
            {
                CHECK(!result);
            }
        }
    }
}

void TestLayoutInitZeroZoneCount()
{
    for (int type = static_cast<int>(ZoneSetLayoutType::Columns); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.zoneCount = 0;

        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            CHECK(!layout->Init(rect, MockMonitor()));
        }
    }

    {
        LayoutData data = kGridLayoutData;
        data.type = ZoneSetLayoutType::Blank;
        data.zoneCount = 0;
        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            CHECK(layout->Init(rect, MockMonitor()));
        }
    }

    {
        LayoutData data = kGridLayoutData;
        data.type = ZoneSetLayoutType::Focus;
        data.zoneCount = 0;
        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            CHECK(layout->Init(rect, MockMonitor()));
        }
    }
}

void TestLayoutInitBigZoneCount()
{
    const int zoneCount = 128;

    for (int type = static_cast<int>(ZoneSetLayoutType::Focus); type < static_cast<int>(ZoneSetLayoutType::Custom); type++)
    {
        LayoutData data = kGridLayoutData;
        data.type = static_cast<ZoneSetLayoutType>(type);
        data.zoneCount = zoneCount;
        data.spacing = 0;

        for (const auto& rect : kWorkAreaRects)
        {
            auto layout = std::make_unique<Layout>(data);
            const bool result = layout->Init(rect, MockMonitor());
            CHECK(result);
            CHECK_ZONES(layout.get(), data.type, static_cast<size_t>(zoneCount), rect);
        }
    }
}

void TestPriorityGridTemplates()
{
    // PriorityGrid 1-11 use the predefined layouts; verify zone counts and validity.
    for (int zoneCount = 1; zoneCount <= 11; zoneCount++)
    {
        LayoutData data = kGridLayoutData;
        data.type = ZoneSetLayoutType::PriorityGrid;
        data.zoneCount = zoneCount;
        data.spacing = 8;

        auto layout = std::make_unique<Layout>(data);
        const bool result = layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());
        CHECK(result);
        CHECK(layout->Zones().size() == static_cast<size_t>(zoneCount));
    }
}

void TestCombinedZoneRange()
{
    LayoutData data = kGridLayoutData;
    data.spacing = 0;
    data.zoneCount = 4;

    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    // 2x2 grid. Zone 0 is top-left, zone 3 is bottom-right; combined range spans all.
    const auto combined = layout->GetCombinedZoneRange({ 0 }, { 3 });
    CHECK(combined.size() == 4);

    // Adjacent cells: 0 and 1 span the top row (2 zones).
    const auto topRow = layout->GetCombinedZoneRange({ 0 }, { 1 });
    CHECK(topRow.size() == 2);

    const RECT combinedRect = layout->GetCombinedZonesRect({ 0, 1, 2, 3 });
    CHECK_RECT((RECT{ 0, 0, 1920, 1080 }), combinedRect);
}

void TestMonitorOrdering()
{
    // Two side-by-side monitors, enumerated in the "wrong" order; OrderMonitors must
    // sort by (top, left) deterministically.
    std::vector<MonitorUtils::MonitorRect> monitors = {
        { MockMonitor(), RECT{ 1920, 0, 3840, 1080 } },
        { MockMonitor(), RECT{ 0, 0, 1920, 1080 } },
    };

    MonitorUtils::OrderMonitors(monitors);
    CHECK(monitors.size() == 2);
    CHECK(monitors[0].second.left == 0);
    CHECK(monitors[1].second.left == 1920);

    const RECT combined = MonitorUtils::GetMonitorsCombinedRect(monitors);
    CHECK_RECT((RECT{ 0, 0, 3840, 1080 }), combined);
}

void TestZoneIndexSetBitmask()
{
    // Round-trip a spread of zone indices, including the 64-bit boundary.
    ZoneIndexSet input = { 0, 1, 63, 64, 65, 127 };
    const auto mask = ZoneIndexSetBitmask::FromIndexSet(input);
    CHECK(mask.part1 == 0x8000000000000003ull);
    CHECK(mask.part2 == 0x8000000000000003ull);

    const ZoneIndexSet roundTripped = mask.ToIndexSet();
    CHECK(roundTripped == input);

    // Out-of-range indices are dropped.
    const ZoneIndexSetBitmask invalidMask = ZoneIndexSetBitmask::FromIndexSet({ -1, 128 });
    CHECK(invalidMask.part1 == 0);
    CHECK(invalidMask.part2 == 0);
}

void TestChooseNextZoneByPosition()
{
    // 2x2 grid over a 1920x1080 work area; rects in work-area coordinates.
    //   zone 0: top-left,  zone 1: top-right,  zone 2: bottom-left,  zone 3: bottom-right
    const std::vector<RECT> allZones = {
        RECT{ 0, 0, 960, 540 },
        RECT{ 960, 0, 1920, 540 },
        RECT{ 0, 540, 960, 1080 },
        RECT{ 960, 540, 1920, 1080 },
    };

    // The app excludes the window's current zone before choosing; mirror that.
    // The result is an INDEX into the passed vector; the "no zone" sentinel is
    // the vector's own size.
    const std::vector<RECT> excludingTopLeft = { allZones[1], allZones[2], allZones[3] };
    const size_t invalidTopLeft = excludingTopLeft.size();

    const std::vector<RECT> excludingTopRight = { allZones[0], allZones[2], allZones[3] };
    const size_t invalidTopRight = excludingTopRight.size();

    const std::vector<RECT> excludingBottomRight = { allZones[0], allZones[1], allZones[2] };
    const size_t invalidBottomRight = excludingBottomRight.size();

    // Window centered on zone 0's center (480, 270).
    const RECT atTopLeft{ 320, 90, 640, 450 };
    CHECK(Util::ChooseNextZoneByPosition(VK_RIGHT, atTopLeft, excludingTopLeft) == 0);
    CHECK(Util::ChooseNextZoneByPosition(VK_DOWN, atTopLeft, excludingTopLeft) == 1);
    CHECK(Util::ChooseNextZoneByPosition(VK_LEFT, atTopLeft, excludingTopLeft) == invalidTopLeft);
    CHECK(Util::ChooseNextZoneByPosition(VK_UP, atTopLeft, excludingTopLeft) == invalidTopLeft);

    // Window centered on zone 3's center (1440, 810).
    const RECT atBottomRight{ 1280, 630, 1600, 990 };
    CHECK(Util::ChooseNextZoneByPosition(VK_LEFT, atBottomRight, excludingBottomRight) == 2);
    CHECK(Util::ChooseNextZoneByPosition(VK_UP, atBottomRight, excludingBottomRight) == 1);

    // Window centered on zone 1's center (1440, 270).
    const RECT atTopRight{ 1280, 90, 1600, 450 };
    CHECK(Util::ChooseNextZoneByPosition(VK_LEFT, atTopRight, excludingTopRight) == 0);
    CHECK(Util::ChooseNextZoneByPosition(VK_DOWN, atTopRight, excludingTopRight) == 2);

    // Unsupported direction.
    CHECK(Util::ChooseNextZoneByPosition(VK_HOME, atTopLeft, excludingTopLeft) == invalidTopLeft);
    CHECK(Util::ChooseNextZoneByPosition(VK_HOME, atTopRight, excludingTopRight) == invalidTopRight);
    CHECK(Util::ChooseNextZoneByPosition(VK_HOME, atBottomRight, excludingBottomRight) == invalidBottomRight);
}

void TestAppZoneHistoryStore()
{
    Paths::EnsureConfigDir();
    const std::wstring testFile = Paths::ConfigDir() + L"\\test-app-zone-history.json";
    AppZoneHistory& history = AppZoneHistory::instance();
    history.SetPathOverride(testFile);
    history.Clear();

    const std::wstring appA = L"C:\\apps\\alpha.exe";
    const std::wstring appB = L"C:\\apps\\beta.exe";
    const ZoneIndexSet zonesA{ 0, 1 };
    const ZoneIndexSet zonesB{ 3 };
    CHECK(history.SetAppLastZones(appA, zonesA));
    CHECK(history.SetAppLastZones(appB, zonesB));
    CHECK(history.GetAppLastZoneIndexSet(appA) == zonesA);
    CHECK(history.GetAppLastZoneIndexSet(appB) == zonesB);

    // Round-trip: reload from disk and verify the entries survived.
    history.LoadData();
    CHECK(history.GetAppLastZoneIndexSet(appA) == zonesA);
    CHECK(history.GetAppLastZoneIndexSet(appB) == zonesB);

    history.Clear();
    CHECK(history.GetAppLastZoneIndexSet(appA).empty());

    DeleteFileW(testFile.c_str());
    history.SetPathOverride(L"");
}

int main()
{
    TestZone();
    TestLayoutBasics();
    TestZoneFromPoint();
    TestZoneFromPointCustom();
    TestZoneFromPointMultizone();
    TestLayoutInitValidValues();
    TestLayoutInitInvalidMonitorInfo();
    TestLayoutInitZeroSpacing();
    TestLayoutInitLargeNegativeSpacing();
    TestLayoutInitHorizontallyBigSpacing();
    TestLayoutInitVerticallyBigSpacing();
    TestLayoutInitZeroZoneCount();
    TestLayoutInitBigZoneCount();
    TestPriorityGridTemplates();
    TestCombinedZoneRange();
    TestMonitorOrdering();
    TestZoneIndexSetBitmask();
    TestChooseNextZoneByPosition();
    TestAppZoneHistoryStore();

    if (g_failures == 0)
    {
        std::cout << "ZoneTests: all checks passed\n";
        return 0;
    }
    std::cerr << "ZoneTests: " << g_failures << " check(s) FAILED\n";
    return 1;
}
