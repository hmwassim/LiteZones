#pragma once

#include "LayoutTypes.h"
#include "Zone.h"

#include <windows.h>

#include <map>
#include <string>

// Mapping zone id to zone
using ZonesMap = std::map<ZoneIndex, Zone>;

enum class OverlappingZonesAlgorithm : int
{
    Smallest,
    Largest,
    Positional,
    ClosestCenter
};

// Parses a settings.json "overlappingZonesAlgorithm" value (smallest/largest/positional/closestCenter).
OverlappingZonesAlgorithm OverlappingZonesAlgorithmFromString(const std::wstring& value);

namespace LayoutConfigurator
{
    ZonesMap Focus(RECT workArea, int zoneCount) noexcept;
    ZonesMap Rows(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Columns(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Grid(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap PriorityGrid(RECT workArea, int zoneCount, int spacing) noexcept;
    ZonesMap Custom(RECT workArea, const FancyZonesDataTypes::CustomLayoutData& data, int spacing) noexcept;
}

// One applied layout: builds zones for a work area and answers geometry queries.
class Layout
{
public:
    explicit Layout(const LayoutData& data);
    ~Layout() = default;

    bool Init(const RECT& workAreaRect, HMONITOR monitor) noexcept;

    GUID Id() const noexcept;
    FancyZonesDataTypes::ZoneSetLayoutType Type() const noexcept;

    const ZonesMap& Zones() const noexcept;
    ZoneIndexSet ZonesFromPoint(POINT pt) const noexcept;
    // Returns all zones spanned by the minimum bounding rectangle containing the two given zone index sets.
    ZoneIndexSet GetCombinedZoneRange(const ZoneIndexSet& initialZones, const ZoneIndexSet& finalZones) const noexcept;
    RECT GetCombinedZonesRect(const ZoneIndexSet& zones);

private:
    const LayoutData m_data;
    ZonesMap m_zones{};
};

// Registry for custom (JSON-defined) layouts, used by Layout::Init when type == Custom.
void SetCustomLayoutData(const GUID& uuid, const FancyZonesDataTypes::CustomLayoutData& data);
void RemoveCustomLayoutData(const GUID& uuid);
