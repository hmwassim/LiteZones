#pragma once

#include "LayoutAssignedWindows.h"
#include "Zone.h"

#include <windows.h>

class ZoneAssignmentStore
{
public:
    void Assign(HWND window, const ZoneIndexSet& zones);
    void Dismiss(HWND window);

    ZoneIndexSet GetZoneIndexSet(HWND window) const;
    bool IsZoneEmpty(ZoneIndex zone) const;

    const std::unordered_map<HWND, ZoneIndexSet>& SnappedWindows() const { return m_layoutWindows.SnappedWindows(); }

private:
    LayoutAssignedWindows m_layoutWindows;
};
