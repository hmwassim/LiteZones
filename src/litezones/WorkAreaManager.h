#pragma once

#include "LayoutTypes.h"
#include "WorkArea.h"

#include <windows.h>

#include <vector>

// Resolves the layout that applies to a monitor: applied-layouts.json first,
// falling back to the caller's default. Custom layouts re-derive their scalars
// from custom-layouts.json and fall back to the default when the uuid is gone.
namespace LayoutResolver
{
    LayoutData Resolve(HMONITOR monitor, bool span, const LayoutData& defaultLayout);
}

// Owns the WorkAreas for the current monitor configuration. Rebuilt on
// display changes or when span mode / layout settings change.
class WorkAreaManager
{
public:
    explicit WorkAreaManager(HINSTANCE hInstance);

    // Re-enumerates monitors and rebuilds work areas (one per monitor, or a
    // single combined work area when span is true). Each monitor's layout comes
    // from applied-layouts.json (via LayoutResolver) or m_defaultLayout. When
    // forceRelayout is false, work areas whose monitor rect is unchanged are
    // preserved (so snapped windows survive resolution changes).
    void Update(bool span, const LayoutData& defaultLayout, bool forceRelayout);

    std::vector<WorkArea>& WorkAreas() { return m_workAreas; }
    const std::vector<WorkArea>& WorkAreas() const { return m_workAreas; }

    WorkArea* WorkAreaFor(HMONITOR monitor);
    const WorkArea* WorkAreaFor(HMONITOR monitor) const;

    // The work area whose rectangle contains the given point (in screen coordinates).
    WorkArea* WorkAreaContainingPoint(POINT pt);
    const WorkArea* WorkAreaContainingPoint(POINT pt) const;

private:
    HINSTANCE m_hInstance = nullptr;
    std::vector<WorkArea> m_workAreas;
};
