#pragma once

#include "LayoutTypes.h"
#include "WorkArea.h"

#include <windows.h>

#include <vector>

// Owns the WorkAreas for the current monitor configuration. Rebuilt on
// display changes or when span mode / layout settings change.
class WorkAreaManager
{
public:
    explicit WorkAreaManager(HINSTANCE hInstance);

    // Re-enumerates monitors and rebuilds work areas (one per monitor, or a
    // single combined work area when span is true). Uses m_defaultLayout for
    // each work area; a per-monitor override (M5) can replace it later.
    void Update(bool span, const LayoutData& defaultLayout);

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
