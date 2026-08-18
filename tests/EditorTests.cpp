#include "TestHarness.h"

#include "../src/layout/CanvasMath.h"
#include "../src/editor/GridData.h"

void TestGridDataZones()
{
    GridLayoutInfo model(2, 2);
    model.rowsPercents() = { 5000, 5000 };
    model.columnsPercents() = { 5000, 5000 };
    model.cellChildMap() = { { 0, 1 }, { 2, 3 } };

    GridData::Grid grid(model);
    CHECK(grid.Zones().size() == 4);
    CHECK(grid.Resizers().size() == 2);
    if (grid.Zones().size() == 4)
    {
        CHECK(grid.Zones()[0].index == 0 && grid.Zones()[0].left == 0 && grid.Zones()[0].top == 0 &&
              grid.Zones()[0].right == 5000 && grid.Zones()[0].bottom == 5000);
        CHECK(grid.Zones()[1].left == 5000 && grid.Zones()[1].right == 10000 && grid.Zones()[1].top == 0);
        CHECK(grid.Zones()[2].left == 0 && grid.Zones()[2].top == 5000 && grid.Zones()[2].bottom == 10000);
        CHECK(grid.Zones()[3].left == 5000 && grid.Zones()[3].top == 5000);
    }
    CHECK(grid.ResizerPosition(0) == 5000);
    CHECK(grid.ResizerPosition(1) == 5000);
    CHECK(grid.BoundarySegments().size() == 4);
}

void TestGridDataDrag()
{
    GridLayoutInfo model(2, 2);
    model.rowsPercents() = { 5000, 5000 };
    model.columnsPercents() = { 5000, 5000 };
    model.cellChildMap() = { { 0, 1 }, { 2, 3 } };
    model.setShowSpacing(true);
    model.setSpacing(16);
    model.setSensitivityRadius(20);

    GridData::Grid grid(model);
    CHECK(grid.CanDrag(1, 1000));
    grid.Drag(1, 1000);
    const std::vector<int> expectedColumns = { 6000, 4000 };
    const std::vector<int> expectedRows = { 5000, 5000 };
    CHECK(model.columnsPercents() == expectedColumns);
    CHECK(model.rowsPercents() == expectedRows);
    CHECK(model.showSpacing());
    CHECK(model.spacing() == 16);
    CHECK(model.sensitivityRadius() == 20);

    CHECK(!grid.CanDrag(1, 5000));
    CHECK(grid.CanDrag(1, 3999));
    CHECK(!grid.CanDrag(1, 4000));

    GridData::Grid grid2(model);
    CHECK(!grid2.CanDrag(0, 5000));
}

void TestGridDataSplit()
{
    GridLayoutInfo model(1, 1);
    model.rowsPercents() = { 10000 };
    model.columnsPercents() = { 10000 };
    model.cellChildMap() = { { 0 } };
    model.setShowSpacing(true);
    model.setSpacing(16);
    model.setSensitivityRadius(20);

    GridData::Grid grid(model);
    CHECK(grid.Zones().size() == 1);
    grid.Split(0, 5000, GridData::Orientation::Horizontal);
    CHECK(grid.Zones().size() == 2);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 1);
    const std::vector<int> expectedRowsSplit = { 5000, 5000 };
    const std::vector<int> expectedColsSplit = { 10000 };
    const std::vector<std::vector<int>> expectedMapSplit = { { 0 }, { 1 } };
    CHECK(model.rowsPercents() == expectedRowsSplit);
    CHECK(model.columnsPercents() == expectedColsSplit);
    CHECK(model.cellChildMap() == expectedMapSplit);
    CHECK(model.showSpacing());
    CHECK(model.spacing() == 16);
    CHECK(model.sensitivityRadius() == 20);

    CHECK(!grid.CanSplit(0, 0, GridData::Orientation::Horizontal));
    CHECK(grid.CanSplit(0, 2500, GridData::Orientation::Horizontal));
    grid.Split(0, 2500, GridData::Orientation::Vertical);
    CHECK(model.columns() == 2);
}

void TestGridDataSplit2x2()
{
    GridLayoutInfo model(1, 1);
    model.rowsPercents() = { 10000 };
    model.columnsPercents() = { 10000 };
    model.cellChildMap() = { { 0 } };

    GridData::Grid grid(model);
    grid.Split2x2(0);
    CHECK(grid.Zones().size() == 4);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 2);
    const std::vector<int> expectedHalves = { 5000, 5000 };
    const std::vector<std::vector<int>> expectedMap2x2 = { { 0, 1 }, { 2, 3 } };
    CHECK(model.rowsPercents() == expectedHalves);
    CHECK(model.columnsPercents() == expectedHalves);
    CHECK(model.cellChildMap() == expectedMap2x2);
}

void TestGridDataMerge()
{
    GridLayoutInfo model(2, 2);
    model.rowsPercents() = { 5000, 5000 };
    model.columnsPercents() = { 5000, 5000 };
    model.cellChildMap() = { { 0, 1 }, { 2, 3 } };

    GridData::Grid grid(model);
    CHECK(grid.MergeClosureIndices({ 1 }).size() == 1);
    CHECK(grid.MergeClosureIndices({ 0, 3 }).size() == 4);

    grid.DoMerge({ 0, 1 });
    CHECK(grid.Zones().size() == 3);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 2);
    const std::vector<std::vector<int>> expectedMapMerged = { { 0, 0 }, { 1, 2 } };
    CHECK(model.cellChildMap() == expectedMapMerged);
    CHECK(grid.BoundarySegments().size() == 3);

    GridLayoutInfo modelAll(2, 2);
    modelAll.rowsPercents() = { 5000, 5000 };
    modelAll.columnsPercents() = { 5000, 5000 };
    modelAll.cellChildMap() = { { 0, 1 }, { 2, 3 } };
    GridData::Grid gridAll(modelAll);
    gridAll.DoMerge({ 0, 1, 2, 3 });
    CHECK(gridAll.Zones().size() == 1);
    CHECK(modelAll.rows() == 1);
    CHECK(modelAll.columns() == 1);
    const std::vector<int> expectedFull = { 10000 };
    const std::vector<std::vector<int>> expectedMapSingle = { { 0 } };
    CHECK(modelAll.rowsPercents() == expectedFull);
    CHECK(modelAll.columnsPercents() == expectedFull);
    CHECK(modelAll.cellChildMap() == expectedMapSingle);
}

void TestCanvasMath()
{
    const RECT zone{ 100, 100, 300, 200 };

    const RECT normalized = CanvasMath::Normalize(RECT{ 300, 200, 100, 100 });
    CHECK_RECT(zone, normalized);

    const RECT moved = CanvasMath::ClampToCanvas(RECT{ -50, -50, 150, 150 }, 1600, 900);
    const RECT movedExpected{ 0, 0, 200, 200 };
    CHECK_RECT(movedExpected, moved);

    const RECT clamped = CanvasMath::ClampToCanvas(RECT{ 1500, 850, 1750, 950 }, 1600, 900);
    const RECT clampedExpected{ 1350, 800, 1600, 900 };
    CHECK_RECT(clampedExpected, clamped);

    const RECT resizedSE = CanvasMath::Resize(zone, CanvasMath::SE, 100, 50);
    const RECT resizedSEExpected{ 100, 100, 400, 250 };
    CHECK_RECT(resizedSEExpected, resizedSE);

    const RECT resizedNW = CanvasMath::Resize(zone, CanvasMath::NW, 100, 50);
    const RECT resizedNWExpected{ 200, 150, 300, 200 };
    CHECK_RECT(resizedNWExpected, resizedNW);

    const RECT resizedE = CanvasMath::Resize(zone, CanvasMath::E, -250, 0);
    CHECK(resizedE.right - resizedE.left == CanvasMath::MinZoneSize);

    const RECT resizedS = CanvasMath::Resize(zone, CanvasMath::S, 0, -500);
    CHECK(resizedS.bottom - resizedS.top == CanvasMath::MinZoneSize);

    const RECT nwHandle = CanvasMath::HandleRect(zone, CanvasMath::NW);
    CHECK(nwHandle.left <= 100 && 100 <= nwHandle.right && nwHandle.top <= 100 && 100 <= nwHandle.bottom);

    CHECK(CanvasMath::HandleHits(zone, CanvasMath::NE, POINT{ 300, 100 }));
    CHECK(CanvasMath::HandleHits(zone, CanvasMath::SE, POINT{ 300, 200 }));
    CHECK(!CanvasMath::HandleHits(zone, CanvasMath::SW, POINT{ 200, 150 }));
}

void TestRectEquality()
{
    CanvasLayoutInfo::Rect a{ 10, 20, 100, 200 };
    CanvasLayoutInfo::Rect b{ 10, 20, 100, 200 };
    CanvasLayoutInfo::Rect c{ 10, 20, 100, 201 };
    CHECK(a == b);
    CHECK(!(a == c));
}

void TestGridLayoutInfoEquality()
{
    GridLayoutInfo a(2, 2);
    a.rowsPercents() = { 5000, 5000 };
    a.columnsPercents() = { 5000, 5000 };
    a.cellChildMap() = { { 0, 1 }, { 2, 3 } };
    a.setShowSpacing(true);
    a.setSpacing(16);
    a.setSensitivityRadius(20);

    GridLayoutInfo b(2, 2);
    b.rowsPercents() = { 5000, 5000 };
    b.columnsPercents() = { 5000, 5000 };
    b.cellChildMap() = { { 0, 1 }, { 2, 3 } };
    b.setShowSpacing(true);
    b.setSpacing(16);
    b.setSensitivityRadius(20);

    GridLayoutInfo c(2, 2);
    c.rowsPercents() = { 5000, 5000 };
    c.columnsPercents() = { 5000, 5000 };
    c.cellChildMap() = { { 0, 1 }, { 2, 3 } };
    c.setShowSpacing(true);
    c.setSpacing(20);
    c.setSensitivityRadius(20);

    CHECK(a == b);
    CHECK(!(a == c));
}

void TestCanvasLayoutInfoEquality()
{
    CanvasLayoutInfo a;
    a.lastWorkAreaWidth = 1920;
    a.lastWorkAreaHeight = 1080;
    a.zones = { { 0, 0, 960, 1080 }, { 960, 0, 1920, 1080 } };
    a.sensitivityRadius = 20;

    CanvasLayoutInfo b;
    b.lastWorkAreaWidth = 1920;
    b.lastWorkAreaHeight = 1080;
    b.zones = { { 0, 0, 960, 1080 }, { 960, 0, 1920, 1080 } };
    b.sensitivityRadius = 20;

    CanvasLayoutInfo c;
    c.lastWorkAreaWidth = 1920;
    c.lastWorkAreaHeight = 1080;
    c.zones = { { 0, 0, 1920, 1080 } };
    c.sensitivityRadius = 20;

    CHECK(a == b);
    CHECK(!(a == c));
}

void TestCustomLayoutDataEquality()
{
    CustomLayoutData a;
    a.name = L"Test";
    a.type = CustomLayoutType::Grid;
    a.grid = GridLayoutInfo(2, 2);
    a.grid.rowsPercents() = { 5000, 5000 };
    a.grid.columnsPercents() = { 5000, 5000 };
    a.grid.cellChildMap() = { { 0, 1 }, { 2, 3 } };

    CustomLayoutData b;
    b.name = L"Test";
    b.type = CustomLayoutType::Grid;
    b.grid = GridLayoutInfo(2, 2);
    b.grid.rowsPercents() = { 5000, 5000 };
    b.grid.columnsPercents() = { 5000, 5000 };
    b.grid.cellChildMap() = { { 0, 1 }, { 2, 3 } };

    CustomLayoutData c;
    c.name = L"Other";
    c.type = CustomLayoutType::Grid;
    c.grid = GridLayoutInfo(2, 2);
    c.grid.rowsPercents() = { 5000, 5000 };
    c.grid.columnsPercents() = { 5000, 5000 };
    c.grid.cellChildMap() = { { 0, 1 }, { 2, 3 } };

    CHECK(a == b);
    CHECK(!(a == c));
}

void RunEditorTests()
{
    TestGridDataZones();
    TestGridDataDrag();
    TestGridDataSplit();
    TestGridDataSplit2x2();
    TestGridDataMerge();
    TestCanvasMath();
    TestRectEquality();
    TestGridLayoutInfoEquality();
    TestCanvasLayoutInfoEquality();
    TestCustomLayoutDataEquality();
}
