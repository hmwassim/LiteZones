#include "LayoutEngine.h"

#include "Settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <map>

using std::min;
using std::max;

namespace
{
    constexpr int C_MULTIPLIER = 10000;

    // PriorityGrid is unique for zoneCount <= 11. For zoneCount > 11 PriorityGrid is same as Grid.
    FancyZonesDataTypes::GridLayoutInfo MakeGrid(int rows, int columns,
                                                 const std::vector<int>& rowsPercents,
                                                 const std::vector<int>& columnsPercents,
                                                 const std::vector<std::vector<int>>& cellChildMap)
    {
        FancyZonesDataTypes::GridLayoutInfo info(rows, columns);
        info.rowsPercents() = rowsPercents;
        info.columnsPercents() = columnsPercents;
        info.cellChildMap() = cellChildMap;
        return info;
    }

    const std::array<FancyZonesDataTypes::GridLayoutInfo, 11> kPredefinedPriorityGridLayouts = {
        /* 1 */ MakeGrid(1, 1, { 10000 }, { 10000 }, { { 0 } }),
        /* 2 */ MakeGrid(1, 2, { 10000 }, { 6667, 3333 }, { { 0, 1 } }),
        /* 3 */ MakeGrid(1, 3, { 10000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 } }),
        /* 4 */ MakeGrid(2, 3, { 5000, 5000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 0, 1, 3 } }),
        /* 5 */ MakeGrid(2, 3, { 5000, 5000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 3, 1, 4 } }),
        /* 6 */ MakeGrid(3, 3, { 3333, 3334, 3333 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 0, 1, 3 }, { 4, 1, 5 } }),
        /* 7 */ MakeGrid(3, 3, { 3333, 3334, 3333 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 3, 1, 4 }, { 5, 1, 6 } }),
        /* 8 */ MakeGrid(3, 4, { 3333, 3334, 3333 }, { 2500, 2500, 2500, 2500 }, { { 0, 1, 2, 3 }, { 4, 1, 2, 5 }, { 6, 1, 2, 7 } }),
        /* 9 */ MakeGrid(3, 4, { 3333, 3334, 3333 }, { 2500, 2500, 2500, 2500 }, { { 0, 1, 2, 3 }, { 4, 1, 2, 5 }, { 6, 1, 7, 8 } }),
        /* 10 */ MakeGrid(3, 4, { 3333, 3334, 3333 }, { 2500, 2500, 2500, 2500 }, { { 0, 1, 2, 3 }, { 4, 1, 5, 6 }, { 7, 1, 8, 9 } }),
        /* 11 */ MakeGrid(3, 4, { 3333, 3334, 3333 }, { 2500, 2500, 2500, 2500 }, { { 0, 1, 2, 3 }, { 4, 1, 5, 6 }, { 7, 8, 9, 10 } }),
    };

    struct GuidLess
    {
        bool operator()(const GUID& lhs, const GUID& rhs) const noexcept
        {
            return memcmp(&lhs, &rhs, sizeof(GUID)) < 0;
        }
    };

    std::map<GUID, FancyZonesDataTypes::CustomLayoutData, GuidLess>& CustomLayouts()
    {
        static std::map<GUID, FancyZonesDataTypes::CustomLayoutData, GuidLess> layouts;
        return layouts;
    }
}

FancyZonesDataTypes::GridLayoutInfo::GridLayoutInfo(int rows, int columns) :
    m_rows(rows),
    m_columns(columns),
    m_rowsPercents(static_cast<size_t>(rows)),
    m_columnsPercents(static_cast<size_t>(columns))
{
}

int FancyZonesDataTypes::GridLayoutInfo::zoneCount() const
{
    return m_cellChildMap.empty() ? 0 : (m_cellChildMap.back().empty() ? 0 : m_cellChildMap.back().back() + 1);
}

OverlappingZonesAlgorithm OverlappingZonesAlgorithmFromString(const std::wstring& value)
{
    if (value == L"smallest")
    {
        return OverlappingZonesAlgorithm::Smallest;
    }
    if (value == L"largest")
    {
        return OverlappingZonesAlgorithm::Largest;
    }
    if (value == L"positional")
    {
        return OverlappingZonesAlgorithm::Positional;
    }
    return OverlappingZonesAlgorithm::ClosestCenter;
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

ZonesMap CalculateGridZones(RECT workArea, const FancyZonesDataTypes::GridLayoutInfo& gridLayoutInfo, int spacing)
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

                top += row == 0 ? spacing : spacing / 2;
                bottom -= maxRow == rows - 1 ? spacing : spacing / 2;
                left += col == 0 ? spacing : spacing / 2;
                right -= maxCol == columns - 1 ? spacing : spacing / 2;

                Zone zone(RECT{ left, top, right, bottom }, i);
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
    ZonesMap Focus(RECT workArea, int zoneCount) noexcept
    {
        ZonesMap zones;

        long left{ 100 };
        long top{ 100 };
        long right{ left + static_cast<long>((workArea.right - workArea.left) * 0.4) };
        long bottom{ top + static_cast<long>((workArea.bottom - workArea.top) * 0.4) };

        RECT focusZoneRect{ left, top, right, bottom };

        const long focusRectXIncrement = (zoneCount <= 1) ? 0 : 50;
        const long focusRectYIncrement = (zoneCount <= 1) ? 0 : 50;

        for (int i = 0; i < zoneCount; i++)
        {
            Zone zone(focusZoneRect, static_cast<ZoneIndex>(zones.size()));
            if (!zone.IsValid() || !AddZone(zone, zones))
            {
                return {};
            }

            focusZoneRect.left += focusRectXIncrement;
            focusZoneRect.right += focusRectXIncrement;
            focusZoneRect.bottom += focusRectYIncrement;
            focusZoneRect.top += focusRectYIncrement;
        }

        return zones;
    }

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

        FancyZonesDataTypes::GridLayoutInfo gridLayoutInfo(rows, columns);

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
        if (zoneCount <= 0)
        {
            return {};
        }

        constexpr size_t predefinedLayoutsCount = kPredefinedPriorityGridLayouts.size();
        if (zoneCount < static_cast<int>(predefinedLayoutsCount))
        {
            return CalculateGridZones(workArea, kPredefinedPriorityGridLayouts[static_cast<size_t>(zoneCount - 1)], spacing);
        }

        return Grid(workArea, zoneCount, spacing);
    }

    ZonesMap Custom(RECT workArea, const FancyZonesDataTypes::CustomLayoutData& zoneSet, int spacing) noexcept
    {
        if (zoneSet.type == FancyZonesDataTypes::CustomLayoutType::Canvas)
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

namespace ZoneSelectionAlgorithms
{
    constexpr int OVERLAPPING_CENTERS_SENSITIVITY = 75;

    template<class CompareF>
    ZoneIndexSet ZoneSelectPriority(const ZonesMap& zones, const ZoneIndexSet& capturedZones, CompareF compare)
    {
        size_t chosen = 0;

        for (size_t i = 1; i < capturedZones.size(); ++i)
        {
            if (compare(zones.at(capturedZones[i]), zones.at(capturedZones[chosen])))
            {
                chosen = i;
            }
        }

        return { capturedZones[chosen] };
    }

    ZoneIndexSet ZoneSelectSubregion(const ZonesMap& zones, const ZoneIndexSet& capturedZones, POINT pt, int sensitivityRadius)
    {
        auto expand = [&](RECT& rect) {
            rect.top -= sensitivityRadius / 2;
            rect.bottom += sensitivityRadius / 2;
            rect.left -= sensitivityRadius / 2;
            rect.right += sensitivityRadius / 2;
        };

        // Compute the overlapped rectangle.
        RECT overlap = zones.at(capturedZones[0]).GetZoneRect();
        expand(overlap);

        for (size_t i = 1; i < capturedZones.size(); ++i)
        {
            RECT current = zones.at(capturedZones[i]).GetZoneRect();
            expand(current);

            overlap.top = max(overlap.top, current.top);
            overlap.left = max(overlap.left, current.left);
            overlap.bottom = min(overlap.bottom, current.bottom);
            overlap.right = min(overlap.right, current.right);
        }

        // Avoid division by zero
        const int width = max(overlap.right - overlap.left, 1L);
        const int height = max(overlap.bottom - overlap.top, 1L);

        const bool verticalSplit = height > width;
        ZoneIndex zoneIndex;

        if (verticalSplit)
        {
            zoneIndex = (static_cast<ZoneIndex>(pt.y) - overlap.top) * static_cast<ZoneIndex>(capturedZones.size()) / height;
        }
        else
        {
            zoneIndex = (static_cast<ZoneIndex>(pt.x) - overlap.left) * static_cast<ZoneIndex>(capturedZones.size()) / width;
        }

        zoneIndex = std::clamp(zoneIndex, static_cast<ZoneIndex>(0), static_cast<ZoneIndex>(capturedZones.size()) - 1);

        return { capturedZones[static_cast<size_t>(zoneIndex)] };
    }

    ZoneIndexSet ZoneSelectClosestCenter(const ZonesMap& zones, const ZoneIndexSet& capturedZones, POINT pt)
    {
        auto getCenter = [](const Zone& zone) {
            const RECT rect = zone.GetZoneRect();
            return POINT{ (rect.right + rect.left) / 2, (rect.top + rect.bottom) / 2 };
        };
        auto pointDifference = [](POINT pt1, POINT pt2) {
            return (pt1.x - pt2.x) * (pt1.x - pt2.x) + (pt1.y - pt2.y) * (pt1.y - pt2.y);
        };
        auto distanceFromCenter = [&](const Zone& zone) {
            const POINT center = getCenter(zone);
            return pointDifference(center, pt);
        };
        auto closerToCenter = [&](const Zone& zone1, const Zone& zone2) {
            if (pointDifference(getCenter(zone1), getCenter(zone2)) > OVERLAPPING_CENTERS_SENSITIVITY)
            {
                return distanceFromCenter(zone1) < distanceFromCenter(zone2);
            }
            else
            {
                return zone1.GetZoneArea() < zone2.GetZoneArea();
            }
        };
        return ZoneSelectPriority(zones, capturedZones, closerToCenter);
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
    const bool isGridType = m_data.type == FancyZonesDataTypes::ZoneSetLayoutType::Columns ||
                            m_data.type == FancyZonesDataTypes::ZoneSetLayoutType::Rows ||
                            m_data.type == FancyZonesDataTypes::ZoneSetLayoutType::Grid ||
                            m_data.type == FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid;

    if (m_data.zoneCount < 0 || (m_data.zoneCount == 0 && isGridType))
    {
        return false;
    }

    const int spacing = m_data.showSpacing ? m_data.spacing : 0;

    switch (m_data.type)
    {
    case FancyZonesDataTypes::ZoneSetLayoutType::Blank:
        m_zones = {};
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::Focus:
        m_zones = LayoutConfigurator::Focus(workArea, m_data.zoneCount);
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::Columns:
        m_zones = LayoutConfigurator::Columns(workArea, m_data.zoneCount, spacing);
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::Rows:
        m_zones = LayoutConfigurator::Rows(workArea, m_data.zoneCount, spacing);
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::Grid:
        m_zones = LayoutConfigurator::Grid(workArea, m_data.zoneCount, spacing);
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid:
        m_zones = LayoutConfigurator::PriorityGrid(workArea, m_data.zoneCount, spacing);
        break;
    case FancyZonesDataTypes::ZoneSetLayoutType::Custom:
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

GUID Layout::Id() const noexcept
{
    return m_data.uuid;
}

FancyZonesDataTypes::ZoneSetLayoutType Layout::Type() const noexcept
{
    return m_data.type;
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

    // If captured zones do not overlap, return all of them.
    // Otherwise, return one of them based on the chosen selection algorithm.
    bool overlap = false;
    for (size_t i = 0; i < capturedZones.size(); ++i)
    {
        for (size_t j = i + 1; j < capturedZones.size(); ++j)
        {
            const auto itI = m_zones.find(capturedZones[i]);
            const auto itJ = m_zones.find(capturedZones[j]);
            if (itI == m_zones.end() || itJ == m_zones.end())
            {
                return {};
            }

            const RECT rectI = itI->second.GetZoneRect();
            const RECT rectJ = itJ->second.GetZoneRect();

            if (max(rectI.top, rectJ.top) + m_data.sensitivityRadius < min(rectI.bottom, rectJ.bottom) &&
                max(rectI.left, rectJ.left) + m_data.sensitivityRadius < min(rectI.right, rectJ.right))
            {
                overlap = true;
                break;
            }
        }
        if (overlap)
        {
            break;
        }
    }

    if (overlap)
    {
        const OverlappingZonesAlgorithm algorithm = OverlappingZonesAlgorithmFromString(Settings::instance().data.overlappingZonesAlgorithm);

        switch (algorithm)
        {
        case OverlappingZonesAlgorithm::Smallest:
            return ZoneSelectionAlgorithms::ZoneSelectPriority(m_zones, capturedZones, [&](const Zone& zone1, const Zone& zone2) { return zone1.GetZoneArea() < zone2.GetZoneArea(); });
        case OverlappingZonesAlgorithm::Largest:
            return ZoneSelectionAlgorithms::ZoneSelectPriority(m_zones, capturedZones, [&](const Zone& zone1, const Zone& zone2) { return zone1.GetZoneArea() > zone2.GetZoneArea(); });
        case OverlappingZonesAlgorithm::Positional:
            return ZoneSelectionAlgorithms::ZoneSelectSubregion(m_zones, capturedZones, pt, m_data.sensitivityRadius);
        case OverlappingZonesAlgorithm::ClosestCenter:
            return ZoneSelectionAlgorithms::ZoneSelectClosestCenter(m_zones, capturedZones, pt);
        }
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
            const RECT rect = it->second.GetZoneRect();
            if (boundingRectEmpty)
            {
                boundingRect = rect;
                boundingRectEmpty = false;
            }
            else
            {
                boundingRect.left = min(boundingRect.left, rect.left);
                boundingRect.top = min(boundingRect.top, rect.top);
                boundingRect.right = max(boundingRect.right, rect.right);
                boundingRect.bottom = max(boundingRect.bottom, rect.bottom);
            }
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

RECT Layout::GetCombinedZonesRect(const ZoneIndexSet& zones)
{
    RECT size{};
    bool sizeEmpty = true;

    for (const ZoneIndex id : zones)
    {
        const auto it = m_zones.find(id);
        if (it != m_zones.end())
        {
            const RECT newSize = it->second.GetZoneRect();
            if (!sizeEmpty)
            {
                size.left = min(size.left, newSize.left);
                size.top = min(size.top, newSize.top);
                size.right = max(size.right, newSize.right);
                size.bottom = max(size.bottom, newSize.bottom);
            }
            else
            {
                size = newSize;
                sizeEmpty = false;
            }
        }
    }

    return size;
}

void SetCustomLayoutData(const GUID& uuid, const FancyZonesDataTypes::CustomLayoutData& data)
{
    CustomLayouts()[uuid] = data;
}

void RemoveCustomLayoutData(const GUID& uuid)
{
    CustomLayouts().erase(uuid);
}
