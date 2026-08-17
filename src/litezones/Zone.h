#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace ZoneConstants
{
    constexpr int MIN_ZONE_EDGE = 0;
}

using ZoneIndex = int64_t;
using ZoneIndexSet = std::vector<ZoneIndex>;

// A zone inside an applied zone layout: a wrapper around a rectangle.
class Zone
{
public:
    Zone(const RECT& zoneRect, const ZoneIndex zoneIndex);

    ZoneIndex Id() const noexcept;
    bool IsValid() const noexcept;
    RECT GetZoneRect() const noexcept;
    long GetZoneArea() const noexcept;

private:
    bool isValid() const noexcept;

    const RECT m_rect;
    const ZoneIndex m_index;
};
