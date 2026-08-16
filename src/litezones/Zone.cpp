#include "Zone.h"

#include <algorithm>

using std::max;

Zone::Zone(const RECT& zoneRect, const ZoneIndex zoneIndex) :
    m_rect(zoneRect),
    m_index(zoneIndex)
{
}

ZoneIndex Zone::Id() const noexcept
{
    return m_index;
}

bool Zone::IsValid() const noexcept
{
    return m_index >= 0 && isValid();
}

RECT Zone::GetZoneRect() const noexcept
{
    return m_rect;
}

long Zone::GetZoneArea() const noexcept
{
    return max(m_rect.bottom - m_rect.top, 0L) * max(m_rect.right - m_rect.left, 0L);
}

bool Zone::isValid() const noexcept
{
    const int width = m_rect.right - m_rect.left;
    const int height = m_rect.bottom - m_rect.top;
    return m_rect.left >= ZoneConstants::MAX_NEGATIVE_SPACING &&
           m_rect.right >= ZoneConstants::MAX_NEGATIVE_SPACING &&
           m_rect.top >= ZoneConstants::MAX_NEGATIVE_SPACING &&
           m_rect.bottom >= ZoneConstants::MAX_NEGATIVE_SPACING &&
           width >= 0 && height >= 0;
}
