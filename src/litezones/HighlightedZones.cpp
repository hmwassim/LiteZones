#include "HighlightedZones.h"

#include "LayoutEngine.h"

#include <utility>

const ZoneIndexSet& HighlightedZones::Zones() const noexcept
{
    return m_highlightZone;
}

bool HighlightedZones::Empty() const noexcept
{
    return m_highlightZone.empty();
}

bool HighlightedZones::Update(const Layout* layout, POINT point, bool selectManyZones) noexcept
{
    if (!layout)
    {
        return false;
    }

    ZoneIndexSet highlightZone = layout->ZonesFromPoint(point);

    if (selectManyZones)
    {
        if (m_initialHighlightZone.empty())
        {
            m_initialHighlightZone = highlightZone;
        }
        else
        {
            highlightZone = layout->GetCombinedZoneRange(m_initialHighlightZone, highlightZone);
        }
    }
    else
    {
        m_initialHighlightZone = {};
    }

    const bool updated = (highlightZone != m_highlightZone);
    m_highlightZone = std::move(highlightZone);
    return updated;
}

void HighlightedZones::Reset() noexcept
{
    m_highlightZone = {};
    m_initialHighlightZone = {};
}
