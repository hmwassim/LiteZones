#include "TestHarness.h"

#include "../../src/litezones/WindowProperties.h"

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

    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    // All 4 zones overlap at (50,50); without overlap disambiguation, all are returned.
    auto zones = layout->ZonesFromPoint(POINT{ 50, 50 });
    CHECK(zones.size() == 4);
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

    auto layout = std::make_unique<Layout>(data);
    layout->Init(RECT{ 0, 0, 1920, 1080 }, MockMonitor());

    auto actual = layout->ZonesFromPoint(POINT{ 50, 100 });
    CHECK(actual.size() == 2);

    Zone zone1({ 0, 0, 100, 100 }, 0);
    Zone zone3({ 0, 100, 100, 200 }, 2);
    CHECK_ZONE(zone1, layout->Zones().at(actual[0]));
    CHECK_ZONE(zone3, layout->Zones().at(actual[1]));
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

void RunZoneMathTests()
{
    TestZone();
    TestZoneFromPoint();
    TestZoneFromPointCustom();
    TestZoneFromPointMultizone();
    TestZoneIndexSetBitmask();
}
