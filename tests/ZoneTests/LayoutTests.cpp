#include "TestHarness.h"

void TestLayoutBasics()
{
    auto layout = std::make_unique<Layout>(kGridLayoutData);

    CHECK(layout->Zones().empty());
    CHECK(layout->ZonesFromPoint(POINT{ 0, 0 }).empty());
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

void RunLayoutTests()
{
    TestLayoutBasics();
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
}
