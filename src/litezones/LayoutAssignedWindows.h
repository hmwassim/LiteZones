#pragma once

#include "Zone.h"

#include <windows.h>

#include <map>
#include <unordered_map>

// Tracks which windows are snapped into which zone index sets for a work area.
class LayoutAssignedWindows
{
public:
    void Assign(HWND window, const ZoneIndexSet& zones);
    void Dismiss(HWND window);

    // Snapped windows for this work area (keyed by HWND).
    const std::unordered_map<HWND, ZoneIndexSet>& SnappedWindows() const { return m_windowIndexSet; }

    // The zone index set the window is snapped to in this work area (empty when not snapped here).
    ZoneIndexSet GetZoneIndexSetFromWindow(HWND window) const;

    bool IsZoneEmpty(ZoneIndex zone) const;

private:
    // Window handle -> zones it is snapped to.
    std::unordered_map<HWND, ZoneIndexSet> m_windowIndexSet;
    // Zone index set -> windows snapped to exactly that set.
    std::map<ZoneIndexSet, std::vector<HWND>> m_windowsByIndexSets;
};
