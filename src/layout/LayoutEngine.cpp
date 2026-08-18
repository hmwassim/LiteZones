#include "LayoutEngine.h"

#include "GuidUtils.h"
#include "LayoutHelpers.h"
#include "Settings.h"

#include <algorithm>
#include <iterator>
#include <map>

using std::min;
using std::max;

namespace
{
    constexpr int C_MULTIPLIER = 10000;

    std::map<GUID, LiteZonesTypes::CustomLayoutData, Util::GuidLess>& CustomLayouts()
    {
        static std::map<GUID, LiteZonesTypes::CustomLayoutData, Util::GuidLess> layouts;
        return layouts;
    }
}

LiteZonesTypes::GridLayoutInfo::GridLayoutInfo(int rows, int columns) :
    m_rows(rows),
    m_columns(columns),
    m_rowsPercents(static_cast<size_t>(rows)),
    m_columnsPercents(static_cast<size_t>(columns))
{
}

int LiteZonesTypes::GridLayoutInfo::zoneCount() const
{
    int maxChild = -1;
    for (const auto& row : m_cellChildMap)
    {
        for (int cell : row)
        {
            if (cell > maxChild)
            {
                maxChild = cell;
            }
        }
    }
    return maxChild + 1;
}

bool AddZone(Zone zone, ZonesMap& zones) noexcept
{
    const auto zoneId = zone.Id();
    if (zones.find(zoneId) != zones.end())
    {
        return false;
    }
    zones.insert({ zoneId, std::move(zone) });
    return true;
}

RECT ApplyGridCellSpacing(RECT rawCellRect, bool isFirstRow, bool isLastRow, bool isFirstCol, bool isLastCol, int spacing) noexcept
{
    rawCellRect.top += isFirstRow ? spacing : spacing / 2;
    rawCellRect.bottom -= isLastRow ? spacing : spacing / 2;
    rawCellRect.left += isFirstCol ? spacing : spacing / 2;
    rawCellRect.right -= isLastCol ? spacing : spacing / 2;
    return rawCellRect;
}

ZonesMap CalculateGridZones(RECT workArea, const LiteZonesTypes::GridLayoutInfo& gridLayoutInfo, int spacing)
{
    ZonesMap zones;

    const long totalWidth = workArea.right - workArea.left;
    const long totalHeight = workArea.bottom - workArea.top;

    struct Info
    {
        long Extent;
        long Start;
        long End;
    };
    std::vector<Info> rowInfo(static_cast<size_t>(gridLayoutInfo.rows()));
    std::vector<Info> columnInfo(static_cast<size_t>(gridLayoutInfo.columns()));

    // Note: The expressions below are carefully written to
    // make the sum of all zones' sizes exactly total{Width|Height}.
    int totalPercents = 0;
    for (int row = 0; row < gridLayoutInfo.rows(); row++)
    {
        rowInfo[static_cast<size_t>(row)].Start = totalPercents * totalHeight / C_MULTIPLIER;
        totalPercents += gridLayoutInfo.rowsPercents()[static_cast<size_t>(row)];
        rowInfo[static_cast<size_t>(row)].End = totalPercents * totalHeight / C_MULTIPLIER;
        rowInfo[static_cast<size_t>(row)].Extent = rowInfo[static_cast<size_t>(row)].End - rowInfo[static_cast<size_t>(row)].Start;
    }

    totalPercents = 0;
    for (int col = 0; col < gridLayoutInfo.columns(); col++)
    {
        columnInfo[static_cast<size_t>(col)].Start = totalPercents * totalWidth / C_MULTIPLIER;
        totalPercents += gridLayoutInfo.columnsPercents()[static_cast<size_t>(col)];
        columnInfo[static_cast<size_t>(col)].End = totalPercents * totalWidth / C_MULTIPLIER;
        columnInfo[static_cast<size_t>(col)].Extent = columnInfo[static_cast<size_t>(col)].End - columnInfo[static_cast<size_t>(col)].Start;
    }

    const int64_t rows = gridLayoutInfo.rows();
    const int64_t columns = gridLayoutInfo.columns();
    for (int64_t row = 0; row < rows; row++)
    {
        for (int64_t col = 0; col < columns; col++)
        {
            const int i = gridLayoutInfo.cellChildMap()[static_cast<size_t>(row)][static_cast<size_t>(col)];
            if (((row == 0) || (gridLayoutInfo.cellChildMap()[static_cast<size_t>(row - 1)][static_cast<size_t>(col)] != i)) &&
                ((col == 0) || (gridLayoutInfo.cellChildMap()[static_cast<size_t>(row)][static_cast<size_t>(col - 1)] != i)))
            {
                long left = columnInfo[static_cast<size_t>(col)].Start;
                long top = rowInfo[static_cast<size_t>(row)].Start;

                int64_t maxRow = row;
                while (((maxRow + 1) < rows) && (gridLayoutInfo.cellChildMap()[static_cast<size_t>(maxRow + 1)][static_cast<size_t>(col)] == i))
                {
                    maxRow++;
                }
                int64_t maxCol = col;
                while (((maxCol + 1) < columns) && (gridLayoutInfo.cellChildMap()[static_cast<size_t>(row)][static_cast<size_t>(maxCol + 1)] == i))
                {
                    maxCol++;
                }

                long right = columnInfo[static_cast<size_t>(maxCol)].End;
                long bottom = rowInfo[static_cast<size_t>(maxRow)].End;

                const RECT spaced = ApplyGridCellSpacing(
                    RECT{ left, top, right, bottom },
                    row == 0, maxRow == rows - 1, col == 0, maxCol == columns - 1,
                    spacing);

                Zone zone(spaced, i);
                if (!zone.IsValid() || !AddZone(zone, zones))
                {
                    return {};
                }
            }
        }
    }

    return zones;
}

namespace LayoutConfigurator
{
    ZonesMap Rows(RECT workArea, int zoneCount, int spacing) noexcept
    {
        if (zoneCount == 0)
        {
            return {};
        }

        ZonesMap zones;

        const long totalWidth = (workArea.right - workArea.left) - (spacing * 2);
        const long totalHeight = (workArea.bottom - workArea.top) - (spacing * (zoneCount + 1));

        long top = spacing;
        long left = spacing;
        long bottom;
        long right;

        // Note: The expressions below are NOT equal to total{Width|Height} / zoneCount and are done
        // like this to make the sum of all zones' sizes exactly total{Width|Height}.
        for (int zoneIndex = 0; zoneIndex < zoneCount; ++zoneIndex)
        {
            right = totalWidth + spacing;
            bottom = top + (zoneIndex + 1) * totalHeight / zoneCount - zoneIndex * totalHeight / zoneCount;

            Zone zone(RECT{ left, top, right, bottom }, static_cast<ZoneIndex>(zones.size()));
            if (!zone.IsValid() || !AddZone(zone, zones))
            {
                return {};
            }

            top = bottom + spacing;
        }

        return zones;
    }

    ZonesMap Columns(RECT workArea, int zoneCount, int spacing) noexcept
    {
        if (zoneCount == 0)
        {
            return {};
        }

        ZonesMap zones;

        const long totalWidth = (workArea.right - workArea.left) - (spacing * (zoneCount + 1));
        const long totalHeight = (workArea.bottom - workArea.top) - (spacing * 2);

        long top = spacing;
        long left = spacing;
        long bottom;
        long right;

        // Note: The expressions below are NOT equal to total{Width|Height} / zoneCount and are done
        // like this to make the sum of all zones' sizes exactly total{Width|Height}.
        for (int zoneIndex = 0; zoneIndex < zoneCount; ++zoneIndex)
        {
            right = left + (zoneIndex + 1) * totalWidth / zoneCount - zoneIndex * totalWidth / zoneCount;
            bottom = totalHeight + spacing;

            Zone zone(RECT{ left, top, right, bottom }, static_cast<ZoneIndex>(zones.size()));
            if (!zone.IsValid() || !AddZone(zone, zones))
            {
                return {};
            }

            left = right + spacing;
        }

        return zones;
    }

    ZonesMap Grid(RECT workArea, int zoneCount, int spacing) noexcept
    {
        if (zoneCount == 0)
        {
            return {};
        }

        int rows = 1;
        int columns = 1;
        while (zoneCount / rows >= rows)
        {
            rows++;
        }
        rows--;
        columns = zoneCount / rows;
        if (zoneCount % rows != 0)
        {
            columns++;
        }

        LiteZonesTypes::GridLayoutInfo gridLayoutInfo(rows, columns);

        // Note: The expressions below are NOT equal to C_MULTIPLIER / {rows|columns} and are done
        // like this to make the sum of all percents exactly C_MULTIPLIER.
        for (int row = 0; row < rows; row++)
        {
            gridLayoutInfo.rowsPercents()[static_cast<size_t>(row)] = C_MULTIPLIER * (row + 1) / rows - C_MULTIPLIER * row / rows;
        }
        for (int col = 0; col < columns; col++)
        {
            gridLayoutInfo.columnsPercents()[static_cast<size_t>(col)] = C_MULTIPLIER * (col + 1) / columns - C_MULTIPLIER * col / columns;
        }

        gridLayoutInfo.cellChildMap().resize(static_cast<size_t>(rows));
        for (int i = 0; i < rows; ++i)
        {
            gridLayoutInfo.cellChildMap()[static_cast<size_t>(i)] = std::vector<int>(columns);
        }

        int index = 0;
        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < columns; col++)
            {
                gridLayoutInfo.cellChildMap()[static_cast<size_t>(row)][static_cast<size_t>(col)] = index++;
                if (index == zoneCount)
                {
                    index--;
                }
            }
        }

        return CalculateGridZones(workArea, gridLayoutInfo, spacing);
    }

    ZonesMap PriorityGrid(RECT workArea, int zoneCount, int spacing) noexcept
    {
        return Grid(workArea, zoneCount, spacing);
    }

    ZonesMap Custom(RECT workArea, const LiteZonesTypes::CustomLayoutData& zoneSet, int spacing) noexcept
    {
        if (zoneSet.type == LiteZonesTypes::CustomLayoutType::Canvas)
        {
            ZonesMap zones;
            const auto& zoneSetInfo = zoneSet.canvas;

            const float width = static_cast<float>(workArea.right - workArea.left);
            const float height = static_cast<float>(workArea.bottom - workArea.top);

            for (const auto& zone : zoneSetInfo.zones)
            {
                const float x = static_cast<float>(zone.x) * width / static_cast<float>(zoneSetInfo.lastWorkAreaWidth);
                const float y = static_cast<float>(zone.y) * height / static_cast<float>(zoneSetInfo.lastWorkAreaHeight);
                const float zoneWidth = static_cast<float>(zone.width) * width / static_cast<float>(zoneSetInfo.lastWorkAreaWidth);
                const float zoneHeight = static_cast<float>(zone.height) * height / static_cast<float>(zoneSetInfo.lastWorkAreaHeight);

                Zone zone_to_add(RECT{ static_cast<long>(x), static_cast<long>(y), static_cast<long>(x + zoneWidth), static_cast<long>(y + zoneHeight) }, static_cast<ZoneIndex>(zones.size()));
                if (!zone_to_add.IsValid() || !AddZone(zone_to_add, zones))
                {
                    return {};
                }
            }

            return zones;
        }

        return CalculateGridZones(workArea, zoneSet.grid, spacing);
    }
}

Layout::Layout(const LayoutData& data) :
    m_data(data)
{
}

bool Layout::Init(const RECT& workArea, HMONITOR monitor) noexcept
{
    (void)monitor;

    // invalid work area
    if ((workArea.right - workArea.left) == 0 || (workArea.bottom - workArea.top) == 0)
    {
        return false;
    }

    // invalid zoneCount, may cause division by zero
    const bool isGridType = m_data.type == LiteZonesTypes::ZoneSetLayoutType::Columns ||
                            m_data.type == LiteZonesTypes::ZoneSetLayoutType::Rows ||
                            m_data.type == LiteZonesTypes::ZoneSetLayoutType::Grid ||
                            m_data.type == LiteZonesTypes::ZoneSetLayoutType::PriorityGrid;

    if (m_data.zoneCount < 0 || (m_data.zoneCount == 0 && isGridType))
    {
        return false;
    }

    const int spacing = m_data.showSpacing ? m_data.spacing : 0;

    switch (m_data.type)
    {
    case LiteZonesTypes::ZoneSetLayoutType::Columns:
        m_zones = LayoutConfigurator::Columns(workArea, m_data.zoneCount, spacing);
        break;
    case LiteZonesTypes::ZoneSetLayoutType::Rows:
        m_zones = LayoutConfigurator::Rows(workArea, m_data.zoneCount, spacing);
        break;
    case LiteZonesTypes::ZoneSetLayoutType::Grid:
        m_zones = LayoutConfigurator::Grid(workArea, m_data.zoneCount, spacing);
        break;
    case LiteZonesTypes::ZoneSetLayoutType::PriorityGrid:
        m_zones = LayoutConfigurator::PriorityGrid(workArea, m_data.zoneCount, spacing);
        break;
    case LiteZonesTypes::ZoneSetLayoutType::Custom:
    {
        const auto it = CustomLayouts().find(m_data.uuid);
        if (it != CustomLayouts().end())
        {
            m_zones = LayoutConfigurator::Custom(workArea, it->second, spacing);
        }
        else
        {
            return false;
        }
    }
    break;
    }

    return m_zones.size() == static_cast<size_t>(m_data.zoneCount);
}

const ZonesMap& Layout::Zones() const noexcept
{
    return m_zones;
}

ZoneIndexSet Layout::ZonesFromPoint(POINT pt) const noexcept
{
    ZoneIndexSet capturedZones;
    ZoneIndexSet strictlyCapturedZones;
    for (const auto& [zoneId, zone] : m_zones)
    {
        const RECT& zoneRect = zone.GetZoneRect();
        if (zoneRect.left - m_data.sensitivityRadius <= pt.x && pt.x <= zoneRect.right + m_data.sensitivityRadius &&
            zoneRect.top - m_data.sensitivityRadius <= pt.y && pt.y <= zoneRect.bottom + m_data.sensitivityRadius)
        {
            capturedZones.emplace_back(zoneId);
        }

        if (zoneRect.left <= pt.x && pt.x < zoneRect.right &&
            zoneRect.top <= pt.y && pt.y < zoneRect.bottom)
        {
            strictlyCapturedZones.emplace_back(zoneId);
        }
    }

    // If only one zone is captured, but it's not strictly captured, don't consider it as captured.
    if (capturedZones.size() == 1 && strictlyCapturedZones.empty())
    {
        return {};
    }

    return capturedZones;
}

ZoneIndexSet Layout::GetCombinedZoneRange(const ZoneIndexSet& initialZones, const ZoneIndexSet& finalZones) const noexcept
{
    ZoneIndexSet combinedZones;
    ZoneIndexSet result;
    std::set_union(begin(initialZones), end(initialZones), begin(finalZones), end(finalZones), std::back_inserter(combinedZones));

    RECT boundingRect{};
    bool boundingRectEmpty = true;

    for (const ZoneIndex zoneId : combinedZones)
    {
        const auto it = m_zones.find(zoneId);
        if (it != m_zones.end())
        {
            LayoutHelpers::ExtendBoundingRect(boundingRect, boundingRectEmpty, it->second.GetZoneRect());
        }
    }

    if (!boundingRectEmpty)
    {
        for (const auto& [zoneId, zone] : m_zones)
        {
            const RECT rect = zone.GetZoneRect();
            if (boundingRect.left <= rect.left && rect.right <= boundingRect.right &&
                boundingRect.top <= rect.top && rect.bottom <= boundingRect.bottom)
            {
                result.push_back(zoneId);
            }
        }
    }

    return result;
}

RECT Layout::GetCombinedZonesRect(const ZoneIndexSet& zones) const
{
    RECT size{};
    bool sizeEmpty = true;

    for (const ZoneIndex id : zones)
    {
        const auto it = m_zones.find(id);
        if (it != m_zones.end())
        {
            LayoutHelpers::ExtendBoundingRect(size, sizeEmpty, it->second.GetZoneRect());
        }
    }

    return size;
}

void SetCustomLayoutData(const GUID& uuid, const LiteZonesTypes::CustomLayoutData& data)
{
    CustomLayouts()[uuid] = data;
}

void RemoveCustomLayoutData(const GUID& uuid)
{
    CustomLayouts().erase(uuid);
}
