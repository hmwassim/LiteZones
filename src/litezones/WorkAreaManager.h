#pragma once

#include "LayoutTypes.h"
#include "WorkArea.h"

#include <windows.h>

#include <vector>

struct SettingsData;

namespace LayoutResolver
{
    LayoutData Resolve(HMONITOR monitor, bool span, const LayoutData& defaultLayout);
}

class WorkAreaManager
{
public:
    WorkAreaManager(HINSTANCE hInstance, const SettingsData& settings);

    void Update(bool span, const LayoutData& defaultLayout, bool forceRelayout);

    std::vector<WorkArea>& WorkAreas() { return m_workAreas; }
    const std::vector<WorkArea>& WorkAreas() const { return m_workAreas; }

    WorkArea* WorkAreaFor(HMONITOR monitor);
    const WorkArea* WorkAreaFor(HMONITOR monitor) const;

    WorkArea* WorkAreaForWindow(HWND window, bool span);
    const WorkArea* WorkAreaForWindow(HWND window, bool span) const;

    WorkArea* WorkAreaContainingPoint(POINT pt);
    const WorkArea* WorkAreaContainingPoint(POINT pt) const;

private:
    HINSTANCE m_hInstance = nullptr;
    const SettingsData& m_settings;
    std::vector<WorkArea> m_workAreas;
};
