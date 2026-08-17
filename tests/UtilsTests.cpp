#include "TestHarness.h"

#include "../src/utils/GuidUtils.h"
#include "../src/layout/LayoutHelpers.h"
#include "../src/platform/MonitorManager.h"
#include "ZoneNavigation.h"

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

void TestLayoutHelpers()
{
    LayoutData def = LayoutHelpers::MakeDefaultLayout();
    CHECK(def.type == LiteZonesTypes::ZoneSetLayoutType::PriorityGrid);
    CHECK(def.zoneCount == DefaultValues::ZoneCount);
    CHECK(def.showSpacing == DefaultValues::ShowSpacing);
    CHECK(def.spacing == DefaultValues::Spacing);
    CHECK(def.sensitivityRadius == DefaultValues::SensitivityRadius);

    LiteZonesTypes::GridLayoutInfo grid3x2 = LayoutHelpers::MakeGridLayout(3, 2);
    CHECK(grid3x2.rows() == 3);
    CHECK(grid3x2.columns() == 2);
    CHECK(grid3x2.zoneCount() == 6);
    CHECK(grid3x2.rowsPercents()[0] + grid3x2.rowsPercents()[1] + grid3x2.rowsPercents()[2] == 10000);
    CHECK(grid3x2.columnsPercents()[0] + grid3x2.columnsPercents()[1] == 10000);
    CHECK(grid3x2.cellChildMap()[0][0] == 0);
    CHECK(grid3x2.cellChildMap()[0][1] == 1);
    CHECK(grid3x2.cellChildMap()[1][0] == 2);
    CHECK(grid3x2.cellChildMap()[2][1] == 5);
    CHECK(grid3x2.showSpacing() == DefaultValues::ShowSpacing);
    CHECK(grid3x2.spacing() == DefaultValues::Spacing);
    CHECK(grid3x2.sensitivityRadius() == DefaultValues::SensitivityRadius);

    LiteZonesTypes::GridLayoutInfo grid1x1 = LayoutHelpers::MakeGridLayout(1, 1);
    CHECK(grid1x1.zoneCount() == 1);
    CHECK(grid1x1.rowsPercents()[0] == 10000);
    CHECK(grid1x1.columnsPercents()[0] == 10000);

    LiteZonesTypes::GridLayoutInfo gridNonSeq(2, 2);
    gridNonSeq.rowsPercents() = { 5000, 5000 };
    gridNonSeq.columnsPercents() = { 6000, 4000 };
    gridNonSeq.cellChildMap() = { {0, 1}, {2, 1} };
    CHECK(gridNonSeq.zoneCount() == 3);

    RECT acc{};
    bool empty = true;
    LayoutHelpers::ExtendBoundingRect(acc, empty, RECT{ 10, 20, 100, 200 });
    CHECK(!empty);
    CHECK(acc.left == 10);
    CHECK(acc.top == 20);
    CHECK(acc.right == 100);
    CHECK(acc.bottom == 200);

    LayoutHelpers::ExtendBoundingRect(acc, empty, RECT{ 5, 25, 150, 180 });
    CHECK(acc.left == 5);
    CHECK(acc.top == 20);
    CHECK(acc.right == 150);
    CHECK(acc.bottom == 200);

    LayoutHelpers::ExtendBoundingRect(acc, empty, RECT{ 30, 10, 80, 250 });
    CHECK(acc.left == 5);
    CHECK(acc.top == 10);
    CHECK(acc.right == 150);
    CHECK(acc.bottom == 250);
}

void RunUtilsTests()
{
    TestMonitorOrdering();
    TestChooseNextZoneByPosition();
    TestLayoutHelpers();
}
