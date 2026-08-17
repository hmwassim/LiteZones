#pragma once

#include "Zone.h"

#include <windows.h>

// Tracks which zones are currently highlighted while dragging a window.
class HighlightedZones
{
public:
    HighlightedZones() = default;

    const ZoneIndexSet& Zones() const noexcept;
    bool Empty() const noexcept;

    // Updates the highlighted zones for the given cursor position (in the layout's
    // work-area coordinates). When selectManyZones is set, highlights the range
    // between the first zone under the cursor and the current one.
    bool Update(const class Layout* layout, POINT point, bool selectManyZones) noexcept;

    void Reset() noexcept;

private:
    ZoneIndexSet m_highlightZone;
    ZoneIndexSet m_initialHighlightZone;
};
