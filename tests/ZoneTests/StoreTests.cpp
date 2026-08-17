#include "TestHarness.h"

#include "../../src/litezones/AppZoneHistory.h"
#include "../../src/litezones/AppliedLayouts.h"
#include "../../src/litezones/CustomLayouts.h"
#include "../../src/litezones/GuidUtils.h"
#include "../../src/litezones/Paths.h"

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

void TestGuidHelpers()
{
    const std::wstring canonical = L"F762BAD6-DAA1-4997-9497-E11DFEB72F21";
    GUID parsed = GUID_NULL;
    CHECK(Util::GuidFromString(canonical, parsed));
    CHECK(Util::GuidToString(parsed) == canonical);
    CHECK(parsed == kLayoutGuid);

    GUID nullRoundTrip = GUID_NULL;
    CHECK(Util::GuidFromString(L"00000000-0000-0000-0000-000000000000", nullRoundTrip));
    CHECK(nullRoundTrip == GUID_NULL);

    CHECK(!Util::GuidFromString(L"not-a-guid", parsed));
    CHECK(!Util::GuidFromString(L"F762BAD6-DAA1-4997-9497-E11DFEB72F2Z", parsed));
}

void TestCustomLayoutsStore()
{
    Paths::EnsureConfigDir();
    const std::wstring testFile = Paths::ConfigDir() + L"\\test-custom-layouts.json";
    CustomLayouts& store = CustomLayouts::instance();
    store.SetPathOverride(testFile);

    const GUID gridGuid = { 0x11111111, 0x1111, 0x1111, { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 } };
    CustomLayoutData grid;
    grid.name = L"My Grid";
    grid.type = CustomLayoutType::Grid;
    grid.grid = GridLayoutInfo(2, 3);
    grid.grid.rowsPercents() = { 5000, 5000 };
    grid.grid.columnsPercents() = { 3333, 3333, 3334 };
    grid.grid.cellChildMap() = { { 0, 1, 2 }, { 0, 1, 2 } };
    grid.grid.m_showSpacing = true;
    grid.grid.m_spacing = 17;
    grid.grid.m_sensitivityRadius = 25;
    CHECK(store.AddLayout(gridGuid, grid));
    CHECK(store.HasLayout(gridGuid));

    auto layout = store.GetLayout(gridGuid);
    CHECK(layout.has_value());
    if (layout.has_value())
    {
        CHECK(layout->type == ZoneSetLayoutType::Custom);
        CHECK(layout->uuid == gridGuid);
        CHECK(layout->zoneCount == 3);
        CHECK(layout->showSpacing == true);
        CHECK(layout->spacing == 17);
        CHECK(layout->sensitivityRadius == 25);
    }

    const GUID canvasGuid = { 0x22222222, 0x2222, 0x2222, { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 } };
    CustomLayoutData canvas;
    canvas.name = L"My Canvas";
    canvas.type = CustomLayoutType::Canvas;
    canvas.canvas.lastWorkAreaWidth = 1920;
    canvas.canvas.lastWorkAreaHeight = 1080;
    canvas.canvas.zones.push_back({ 0, 0, 960, 1080 });
    canvas.canvas.zones.push_back({ 960, 0, 960, 540 });
    canvas.canvas.sensitivityRadius = 20;
    CHECK(store.AddLayout(canvasGuid, canvas));

    // Round-trip from disk.
    store.LoadData();
    CHECK(store.HasLayout(gridGuid));
    CHECK(store.HasLayout(canvasGuid));
    auto gridReloaded = store.GetLayout(gridGuid);
    CHECK(gridReloaded.has_value());
    if (gridReloaded.has_value())
    {
        CHECK(gridReloaded->zoneCount == 3);
        CHECK(gridReloaded->spacing == 17);
    }
    auto canvasReloaded = store.GetLayout(canvasGuid);
    CHECK(canvasReloaded.has_value());
    if (canvasReloaded.has_value())
    {
        CHECK(canvasReloaded->zoneCount == 2);
    }
    const auto* canvasData = store.GetCustomLayoutData(canvasGuid);
    CHECK(canvasData != nullptr);
    if (canvasData)
    {
        CHECK(canvasData->canvas.zones.size() == 2);
        CHECK(canvasData->canvas.zones[0].width == 960);
    }

    store.DeleteLayout(gridGuid);
    CHECK(!store.HasLayout(gridGuid));
    CHECK(store.HasLayout(canvasGuid));
    store.DeleteLayout(canvasGuid);
    CHECK(!store.HasLayout(canvasGuid));

    DeleteFileW(testFile.c_str());
    store.SetPathOverride(L"");
}

void TestCustomLayoutsPercentScaling()
{
    Paths::EnsureConfigDir();
    const std::wstring testFile = Paths::ConfigDir() + L"\\test-custom-layouts-scale.json";
    const std::wstring text =
        L"{\n"
        L"  \"custom-layouts\": [\n"
        L"    {\n"
        L"      \"uuid\": \"00000000-0000-0000-0000-0000000000A1\",\n"
        L"      \"name\": \"Float Grid\",\n"
        L"      \"type\": \"grid\",\n"
        L"      \"info\": {\n"
        L"        \"rows\": 2,\n"
        L"        \"columns\": 3,\n"
        L"        \"rows-percentage\": [50, 50],\n"
        L"        \"columns-percentage\": [33.33, 33.33, 33.34],\n"
        L"        \"cell-child-map\": [[0, 1, 2], [0, 1, 2]],\n"
        L"        \"show-spacing\": true,\n"
        L"        \"spacing\": 16,\n"
        L"        \"sensitivity-radius\": 20\n"
        L"      }\n"
        L"    }\n"
        L"  ]\n"
        L"}\n";
    CHECK(Paths::WriteTextFile(testFile, text, /*crlf=*/false));

    CustomLayouts& store = CustomLayouts::instance();
    store.SetPathOverride(testFile);
    store.LoadData();

    const GUID uuid = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA1 } };
    CHECK(store.HasLayout(uuid));
    const auto* data = store.GetCustomLayoutData(uuid);
    CHECK(data != nullptr);
    if (data)
    {
        CHECK(data->grid.columns() == 3);
        int sum = 0;
        for (const int value : data->grid.columnsPercents())
        {
            sum += value;
        }
        CHECK(sum == 10000);
        CHECK(data->grid.columnsPercents()[0] == 3333);
        CHECK(data->grid.columnsPercents()[2] == 3334);
    }

    DeleteFileW(testFile.c_str());
    store.SetPathOverride(L"");
}

void TestAppliedLayoutsStore()
{
    Paths::EnsureConfigDir();
    const std::wstring testFile = Paths::ConfigDir() + L"\\test-applied-layouts.json";
    AppliedLayouts& store = AppliedLayouts::instance();
    store.SetPathOverride(testFile);

    const std::wstring keyA = L"\\\\?\\DISPLAY#MON123#5&12345&0&UID1#{e6f07b5f-ee97-4a90-b076-33f57bf4ba85}|5&12345&0&UID1";
    const std::wstring keyB = L"\\\\?\\DISPLAY#MON456#5&67890&0&UID2#{e6f07b5f-ee97-4a90-b076-33f57bf4ba85}|5&67890&0&UID2";

    LayoutData gridLayout;
    gridLayout.uuid = { 0x33333333, 0x3333, 0x3333, { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 } };
    gridLayout.type = ZoneSetLayoutType::Grid;
    gridLayout.showSpacing = true;
    gridLayout.spacing = 12;
    gridLayout.zoneCount = 6;
    gridLayout.sensitivityRadius = 30;

    LayoutData priorityLayout;
    priorityLayout.type = ZoneSetLayoutType::PriorityGrid;
    priorityLayout.zoneCount = 3;
    priorityLayout.showSpacing = true;
    priorityLayout.spacing = 16;
    priorityLayout.sensitivityRadius = 20;

    store.ApplyLayout(keyA, gridLayout);
    store.ApplyLayout(keyB, priorityLayout);
    store.SaveData();

    store.LoadData();
    auto a = store.GetDeviceLayout(keyA);
    CHECK(a.has_value());
    if (a.has_value())
    {
        CHECK(a->type == ZoneSetLayoutType::Grid);
        CHECK(a->zoneCount == 6);
        CHECK(a->spacing == 12);
        CHECK(a->uuid == gridLayout.uuid);
    }
    auto b = store.GetDeviceLayout(keyB);
    CHECK(b.has_value());
    if (b.has_value())
    {
        CHECK(b->type == ZoneSetLayoutType::PriorityGrid);
    }
    CHECK(!store.GetDeviceLayout(L"nonexistent").has_value());

    store.ClearDeviceLayout(keyA);
    CHECK(!store.GetDeviceLayout(keyA).has_value());

    DeleteFileW(testFile.c_str());
    store.SetPathOverride(L"");
}

void RunStoreTests()
{
    TestAppZoneHistoryStore();
    TestGuidHelpers();
    TestCustomLayoutsStore();
    TestCustomLayoutsPercentScaling();
    TestAppliedLayoutsStore();
}
