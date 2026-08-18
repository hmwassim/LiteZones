#pragma once

#include "LayoutTypes.h"
#include "Zone.h"

#include <windows.h>

#include <map>
#include <string>

// Mapping zone id to zone
using ZonesMap = std::map<ZoneIndex, Zone>;

// Shared by LayoutConfigurator::Grid/PriorityGrid/Custom(grid) and by the
// editor's live GridEdit preview, so both compute identical pixel rects
// (including spacing) for the same GridLayoutInfo + work area + spacing.
ZonesMap CalculateGridZones(RECT workArea, const LiteZonesTypes::GridLayoutInfo& gridLayoutInfo, int spacing);

// Insets a single grid cell's raw rect by the zone spacing: a full `spacing`
// margin on edges that touch the outside of the grid, and `spacing / 2` on
// edges shared with a neighboring cell (so the gap between two adjacent
// zones totals `spacing`). Factored out of CalculateGridZones so the editor
// can reuse the exact same math per-cell without CalculateGridZones' all-or-
// nothing validity check (which would blank the whole preview if a single
// cell were briefly degenerate mid-drag).
RECT ApplyGridCellSpacing(RECT rawCellRect, bool isFirstRow, bool isLastRow, bool isFirstCol, bool isLastCol, int spacing) noexcept;

namespace LayoutConfigurator
{
    ZonesMap Rows(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Columns(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Grid(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap PriorityGrid(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Custom(RECT workArea, const LiteZonesTypes::CustomLayoutData& data, int spacing) noexcept;
}

// One applied layout: builds zones for a work area and answers geometry queries.
class Layout
{
public:
    explicit Layout(const LayoutData& data);
    ~Layout() = default;

    bool Init(const RECT& workAreaRect, HMONITOR monitor) noexcept;

    const ZonesMap& Zones() const noexcept;
    ZoneIndexSet ZonesFromPoint(POINT pt) const noexcept;
    // Returns all zones spanned by the minimum bounding rectangle containing the two given zone index sets.
    ZoneIndexSet GetCombinedZoneRange(const ZoneIndexSet& initialZones, const ZoneIndexSet& finalZones) const noexcept;
    RECT GetCombinedZonesRect(const ZoneIndexSet& zones) const;

private:
    const LayoutData m_data;
    ZonesMap m_zones{};
};

// Registry for custom (JSON-defined) layouts, used by Layout::Init when type == Custom.
void SetCustomLayoutData(const GUID& uuid, const LiteZonesTypes::CustomLayoutData& data);
void RemoveCustomLayoutData(const GUID& uuid);
