#include "CustomLayouts.h"

#include "LayoutEngine.h"
#include "Paths.h"
#include "json.h"

namespace
{
    const std::wstring kCustomLayouts = L"custom-layouts";
    const std::wstring kUuid = L"uuid";
    const std::wstring kName = L"name";
    const std::wstring kType = L"type";
    const std::wstring kInfo = L"info";
    const std::wstring kGridType = L"grid";
    const std::wstring kCanvasType = L"canvas";

    const std::wstring kRows = L"rows";
    const std::wstring kColumns = L"columns";
    const std::wstring kRowsPercentage = L"rows-percentage";
    const std::wstring kColumnsPercentage = L"columns-percentage";
    const std::wstring kCellChildMap = L"cell-child-map";
    const std::wstring kShowSpacing = L"show-spacing";
    const std::wstring kSpacing = L"spacing";
    const std::wstring kSensitivityRadius = L"sensitivity-radius";
    const std::wstring kRefWidth = L"ref-width";
    const std::wstring kRefHeight = L"ref-height";
    const std::wstring kZones = L"zones";
    const std::wstring kX = L"X";
    const std::wstring kY = L"Y";
    const std::wstring kWidth = L"width";
    const std::wstring kHeight = L"height";

    // Parses a percent array into 0..10000 units. Accepts both the FancyZones
    // 0..10000 form and a plain 0..100 form (auto-detected by the total), then
    // forces the integer sum to be exactly 10000 by adjusting the last element.
    bool ParsePercents(const Json& arr, std::vector<int>& out)
    {
        if (arr.type() != Json::Type::Array || arr.Size() == 0)
        {
            return false;
        }

        std::vector<double> values;
        values.reserve(arr.Size());
        double sum = 0.0;
        for (size_t i = 0; i < arr.Size(); ++i)
        {
            const double value = arr.At(i).AsNumber();
            if (value <= 0.0)
            {
                return false;
            }
            values.push_back(value);
            sum += value;
        }

        const double scale = (sum <= 100.0 + 1e-6) ? 100.0 : 1.0;
        std::vector<int> result;
        result.reserve(values.size());
        int total = 0;
        for (const double value : values)
        {
            int converted = static_cast<int>(value * scale + 0.5);
            if (converted < 1)
            {
                converted = 1;
            }
            total += converted;
            result.push_back(converted);
        }

        if (total != 10000)
        {
            result.back() += (10000 - total);
            if (result.back() < 1)
            {
                return false;
            }
        }

        out = std::move(result);
        return true;
    }

    bool ParseGridInfo(const Json& info, FancyZonesDataTypes::GridLayoutInfo& out)
    {
        const int rows = static_cast<int>(info.At(kRows).AsNumber());
        const int columns = static_cast<int>(info.At(kColumns).AsNumber());
        if (rows <= 0 || columns <= 0)
        {
            return false;
        }

        std::vector<int> rowPercents;
        std::vector<int> columnPercents;
        if (!ParsePercents(info.At(kRowsPercentage), rowPercents) || rowPercents.size() != static_cast<size_t>(rows) ||
            !ParsePercents(info.At(kColumnsPercentage), columnPercents) || columnPercents.size() != static_cast<size_t>(columns))
        {
            return false;
        }

        const Json& cellMap = info.At(kCellChildMap);
        if (cellMap.type() != Json::Type::Array || cellMap.Size() != static_cast<size_t>(rows))
        {
            return false;
        }

        std::vector<std::vector<int>> map;
        map.reserve(static_cast<size_t>(rows));
        int maxChild = -1;
        for (size_t r = 0; r < cellMap.Size(); ++r)
        {
            const Json& row = cellMap.At(r);
            if (row.type() != Json::Type::Array || row.Size() != static_cast<size_t>(columns))
            {
                return false;
            }
            std::vector<int> cells;
            cells.reserve(static_cast<size_t>(columns));
            for (size_t c = 0; c < row.Size(); ++c)
            {
                const int child = static_cast<int>(row.At(c).AsNumber());
                if (child < 0)
                {
                    return false;
                }
                maxChild = (child > maxChild) ? child : maxChild;
                cells.push_back(child);
            }
            map.push_back(std::move(cells));
        }

        FancyZonesDataTypes::GridLayoutInfo infoOut(rows, columns);
        infoOut.rowsPercents() = std::move(rowPercents);
        infoOut.columnsPercents() = std::move(columnPercents);
        infoOut.cellChildMap() = std::move(map);
        infoOut.m_showSpacing = info.At(kShowSpacing).AsBool(DefaultValues::ShowSpacing);
        infoOut.m_spacing = static_cast<int>(info.At(kSpacing).AsNumber(DefaultValues::Spacing));
        infoOut.m_sensitivityRadius = static_cast<int>(info.At(kSensitivityRadius).AsNumber(DefaultValues::SensitivityRadius));

        // Child indices must be contiguous 0..maxChild (each cell references an
        // existing zone), which the zone-count invariant below captures.
        if (infoOut.zoneCount() != maxChild + 1)
        {
            return false;
        }

        out = std::move(infoOut);
        return true;
    }

    bool ParseCanvasInfo(const Json& info, FancyZonesDataTypes::CanvasLayoutInfo& out)
    {
        const int refWidth = static_cast<int>(info.At(kRefWidth).AsNumber());
        const int refHeight = static_cast<int>(info.At(kRefHeight).AsNumber());
        if (refWidth <= 0 || refHeight <= 0)
        {
            return false;
        }

        const Json& zones = info.At(kZones);
        if (zones.type() != Json::Type::Array || zones.Size() == 0)
        {
            return false;
        }

        FancyZonesDataTypes::CanvasLayoutInfo infoOut;
        infoOut.lastWorkAreaWidth = refWidth;
        infoOut.lastWorkAreaHeight = refHeight;
        for (size_t i = 0; i < zones.Size(); ++i)
        {
            const Json& zone = zones.At(i);
            if (zone.type() != Json::Type::Object)
            {
                return false;
            }
            FancyZonesDataTypes::CanvasLayoutInfo::Rect rect;
            rect.x = static_cast<int>(zone.At(kX).AsNumber());
            rect.y = static_cast<int>(zone.At(kY).AsNumber());
            rect.width = static_cast<int>(zone.At(kWidth).AsNumber());
            rect.height = static_cast<int>(zone.At(kHeight).AsNumber());
            if (rect.width <= 0 || rect.height <= 0)
            {
                return false;
            }
            infoOut.zones.push_back(rect);
        }
        infoOut.sensitivityRadius = static_cast<int>(info.At(kSensitivityRadius).AsNumber(DefaultValues::SensitivityRadius));

        out = std::move(infoOut);
        return true;
    }

    Json SerializeGridInfo(const FancyZonesDataTypes::GridLayoutInfo& grid)
    {
        Json info = Json::MakeObject();
        info.Set(kRows, static_cast<double>(grid.rows()));
        info.Set(kColumns, static_cast<double>(grid.columns()));

        Json rowPercents = Json::MakeArray();
        for (const int value : grid.rowsPercents())
        {
            rowPercents.Push(Json::MakeNumber(static_cast<double>(value)));
        }
        info.Set(kRowsPercentage, rowPercents);

        Json columnPercents = Json::MakeArray();
        for (const int value : grid.columnsPercents())
        {
            columnPercents.Push(Json::MakeNumber(static_cast<double>(value)));
        }
        info.Set(kColumnsPercentage, columnPercents);

        Json cellMap = Json::MakeArray();
        for (const auto& row : grid.cellChildMap())
        {
            Json cells = Json::MakeArray();
            for (const int value : row)
            {
                cells.Push(Json::MakeNumber(static_cast<double>(value)));
            }
            cellMap.Push(cells);
        }
        info.Set(kCellChildMap, cellMap);

        info.Set(kShowSpacing, grid.showSpacing());
        info.Set(kSpacing, static_cast<double>(grid.spacing()));
        info.Set(kSensitivityRadius, static_cast<double>(grid.sensitivityRadius()));
        return info;
    }

    Json SerializeCanvasInfo(const FancyZonesDataTypes::CanvasLayoutInfo& canvas)
    {
        Json info = Json::MakeObject();
        info.Set(kRefWidth, static_cast<double>(canvas.lastWorkAreaWidth));
        info.Set(kRefHeight, static_cast<double>(canvas.lastWorkAreaHeight));

        Json zones = Json::MakeArray();
        for (const auto& zone : canvas.zones)
        {
            Json zoneJson = Json::MakeObject();
            zoneJson.Set(kX, static_cast<double>(zone.x));
            zoneJson.Set(kY, static_cast<double>(zone.y));
            zoneJson.Set(kWidth, static_cast<double>(zone.width));
            zoneJson.Set(kHeight, static_cast<double>(zone.height));
            zones.Push(zoneJson);
        }
        info.Set(kZones, zones);

        info.Set(kSensitivityRadius, static_cast<double>(canvas.sensitivityRadius));
        return info;
    }
}

CustomLayouts& CustomLayouts::instance()
{
    static CustomLayouts layouts;
    return layouts;
}

std::wstring CustomLayouts::FilePath() const
{
    return m_pathOverride.empty() ? Paths::CustomLayoutsFile() : m_pathOverride;
}

void CustomLayouts::LoadData()
{
    std::wstring text;
    if (!Paths::ReadTextFile(FilePath(), text))
    {
        return;
    }

    Json root;
    if (!Json::Parse(text, root))
    {
        return;
    }

    CustomLayoutMap fresh;
    const Json& entries = root.At(kCustomLayouts);
    if (entries.type() == Json::Type::Array)
    {
        for (size_t i = 0; i < entries.Size(); ++i)
        {
            const Json& entry = entries.At(i);
            if (entry.type() != Json::Type::Object)
            {
                continue;
            }

            GUID uuid = GUID_NULL;
            if (!Util::GuidFromString(entry.At(kUuid).AsString(), uuid))
            {
                continue;
            }
            const std::wstring& name = entry.At(kName).AsString();
            if (name.empty())
            {
                continue;
            }
            const Json& info = entry.At(kInfo);
            if (info.type() != Json::Type::Object)
            {
                continue;
            }

            FancyZonesDataTypes::CustomLayoutData data;
            data.name = name;
            const std::wstring& type = entry.At(kType).AsString();
            if (type == kGridType)
            {
                data.type = FancyZonesDataTypes::CustomLayoutType::Grid;
                if (!ParseGridInfo(info, data.grid))
                {
                    continue;
                }
            }
            else if (type == kCanvasType)
            {
                data.type = FancyZonesDataTypes::CustomLayoutType::Canvas;
                if (!ParseCanvasInfo(info, data.canvas))
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            fresh[uuid] = std::move(data);
        }
    }

    m_layouts = std::move(fresh);
    SyncRegistry();
}

void CustomLayouts::SaveData() const
{
    Json root = Json::MakeObject();
    Json entries = Json::MakeArray();
    for (const auto& [uuid, data] : m_layouts)
    {
        Json entry = Json::MakeObject();
        entry.Set(kUuid, Util::GuidToString(uuid));
        entry.Set(kName, data.name);
        if (data.type == FancyZonesDataTypes::CustomLayoutType::Grid)
        {
            entry.Set(kType, kGridType);
            entry.Set(kInfo, SerializeGridInfo(data.grid));
        }
        else
        {
            entry.Set(kType, kCanvasType);
            entry.Set(kInfo, SerializeCanvasInfo(data.canvas));
        }
        entries.Push(entry);
    }
    root.Set(kCustomLayouts, entries);

    Paths::WriteTextFile(FilePath(), root.SerializeIndented(), /*crlf=*/false);
}

bool CustomLayouts::HasLayout(const GUID& uuid) const
{
    return m_layouts.find(uuid) != m_layouts.end();
}

std::optional<LayoutData> CustomLayouts::GetLayout(const GUID& uuid) const
{
    const auto it = m_layouts.find(uuid);
    if (it == m_layouts.end())
    {
        return std::nullopt;
    }

    LayoutData layout;
    layout.uuid = uuid;
    layout.type = FancyZonesDataTypes::ZoneSetLayoutType::Custom;
    if (it->second.type == FancyZonesDataTypes::CustomLayoutType::Grid)
    {
        const auto& grid = it->second.grid;
        layout.sensitivityRadius = grid.sensitivityRadius();
        layout.showSpacing = grid.showSpacing();
        layout.spacing = grid.spacing();
        layout.zoneCount = grid.zoneCount();
    }
    else
    {
        const auto& canvas = it->second.canvas;
        layout.sensitivityRadius = canvas.sensitivityRadius;
        layout.zoneCount = static_cast<int>(canvas.zones.size());
    }
    return layout;
}

const FancyZonesDataTypes::CustomLayoutData* CustomLayouts::GetCustomLayoutData(const GUID& uuid) const
{
    const auto it = m_layouts.find(uuid);
    return it == m_layouts.end() ? nullptr : &it->second;
}

bool CustomLayouts::AddLayout(const GUID& uuid, const FancyZonesDataTypes::CustomLayoutData& data)
{
    if (data.name.empty())
    {
        return false;
    }
    m_layouts[uuid] = data;
    SetCustomLayoutData(uuid, data);
    SaveData();
    return true;
}

void CustomLayouts::DeleteLayout(const GUID& uuid)
{
    if (m_layouts.erase(uuid) > 0)
    {
        RemoveCustomLayoutData(uuid);
        SaveData();
    }
}

void CustomLayouts::SyncRegistry() const
{
    for (const auto& [uuid, data] : m_layouts)
    {
        SetCustomLayoutData(uuid, data);
    }
}
