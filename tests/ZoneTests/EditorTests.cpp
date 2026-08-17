#include "TestHarness.h"

#include "../../src/litezones/CanvasMath.h"
#include "../../src/litezones/GridData.h"

void TestGridDataZones()
{
    GridLayoutInfo model(2, 2);
    model.m_rowsPercents = { 5000, 5000 };
    model.m_columnsPercents = { 5000, 5000 };
    model.m_cellChildMap = { { 0, 1 }, { 2, 3 } };

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
    model.m_rowsPercents = { 5000, 5000 };
    model.m_columnsPercents = { 5000, 5000 };
    model.m_cellChildMap = { { 0, 1 }, { 2, 3 } };
    model.m_showSpacing = true;
    model.m_spacing = 16;
    model.m_sensitivityRadius = 20;

    GridData::Grid grid(model);
    CHECK(grid.CanDrag(1, 1000));
    grid.Drag(1, 1000);
    const std::vector<int> expectedColumns = { 6000, 4000 };
    const std::vector<int> expectedRows = { 5000, 5000 };
    CHECK(model.columnsPercents() == expectedColumns);
    CHECK(model.rowsPercents() == expectedRows);
    CHECK(model.m_showSpacing);
    CHECK(model.m_spacing == 16);
    CHECK(model.m_sensitivityRadius == 20);

    CHECK(!grid.CanDrag(1, 5000));
    CHECK(grid.CanDrag(1, 3999));
    CHECK(!grid.CanDrag(1, 4000));

    GridData::Grid grid2(model);
    CHECK(!grid2.CanDrag(0, 5000));
}

void TestGridDataSplit()
{
    GridLayoutInfo model(1, 1);
    model.m_rowsPercents = { 10000 };
    model.m_columnsPercents = { 10000 };
    model.m_cellChildMap = { { 0 } };
    model.m_showSpacing = true;
    model.m_spacing = 16;
    model.m_sensitivityRadius = 20;

    GridData::Grid grid(model);
    CHECK(grid.Zones().size() == 1);
    grid.Split(0, 5000, GridData::Orientation::Horizontal);
    CHECK(grid.Zones().size() == 2);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 1);
    const std::vector<int> expectedRowsSplit = { 5000, 5000 };
    const std::vector<int> expectedColsSplit = { 10000 };
    const std::vector<std::vector<int>> expectedMapSplit = { { 0 }, { 1 } };
    CHECK(model.m_rowsPercents == expectedRowsSplit);
    CHECK(model.m_columnsPercents == expectedColsSplit);
    CHECK(model.m_cellChildMap == expectedMapSplit);
    CHECK(model.m_showSpacing);
    CHECK(model.m_spacing == 16);
    CHECK(model.m_sensitivityRadius == 20);

    CHECK(!grid.CanSplit(0, 0, GridData::Orientation::Horizontal));
    CHECK(grid.CanSplit(0, 2500, GridData::Orientation::Horizontal));
    grid.Split(0, 2500, GridData::Orientation::Vertical);
    CHECK(model.columns() == 2);
}

void TestGridDataSplit2x2()
{
    GridLayoutInfo model(1, 1);
    model.m_rowsPercents = { 10000 };
    model.m_columnsPercents = { 10000 };
    model.m_cellChildMap = { { 0 } };

    GridData::Grid grid(model);
    grid.Split2x2(0);
    CHECK(grid.Zones().size() == 4);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 2);
    const std::vector<int> expectedHalves = { 5000, 5000 };
    const std::vector<std::vector<int>> expectedMap2x2 = { { 0, 1 }, { 2, 3 } };
    CHECK(model.m_rowsPercents == expectedHalves);
    CHECK(model.m_columnsPercents == expectedHalves);
    CHECK(model.m_cellChildMap == expectedMap2x2);
}

void TestGridDataMerge()
{
    GridLayoutInfo model(2, 2);
    model.m_rowsPercents = { 5000, 5000 };
    model.m_columnsPercents = { 5000, 5000 };
    model.m_cellChildMap = { { 0, 1 }, { 2, 3 } };

    GridData::Grid grid(model);
    CHECK(grid.MergeClosureIndices({ 1 }).size() == 1);
    CHECK(grid.MergeClosureIndices({ 0, 3 }).size() == 4);

    grid.DoMerge({ 0, 1 });
    CHECK(grid.Zones().size() == 3);
    CHECK(model.rows() == 2);
    CHECK(model.columns() == 2);
    const std::vector<std::vector<int>> expectedMapMerged = { { 0, 0 }, { 1, 2 } };
    CHECK(model.m_cellChildMap == expectedMapMerged);
    CHECK(grid.BoundarySegments().size() == 3);

    GridLayoutInfo modelAll(2, 2);
    modelAll.m_rowsPercents = { 5000, 5000 };
    modelAll.m_columnsPercents = { 5000, 5000 };
    modelAll.m_cellChildMap = { { 0, 1 }, { 2, 3 } };
    GridData::Grid gridAll(modelAll);
    gridAll.DoMerge({ 0, 1, 2, 3 });
    CHECK(gridAll.Zones().size() == 1);
    CHECK(modelAll.rows() == 1);
    CHECK(modelAll.columns() == 1);
    const std::vector<int> expectedFull = { 10000 };
    const std::vector<std::vector<int>> expectedMapSingle = { { 0 } };
    CHECK(modelAll.m_rowsPercents == expectedFull);
    CHECK(modelAll.m_columnsPercents == expectedFull);
    CHECK(modelAll.m_cellChildMap == expectedMapSingle);
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

void RunEditorTests()
{
    TestGridDataZones();
    TestGridDataDrag();
    TestGridDataSplit();
    TestGridDataSplit2x2();
    TestGridDataMerge();
    TestCanvasMath();
}
