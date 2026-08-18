#pragma once

#include "LayoutEngine.h"
#include "ZoneAssignmentStore.h"

#include <windows.h>

#include <memory>

struct SettingsData;

class WorkArea
{
public:
    WorkArea(HINSTANCE hInstance, HMONITOR monitor, const RECT& workAreaRect, const SettingsData& settings);
    ~WorkArea();

    WorkArea(WorkArea&&) noexcept;
    WorkArea& operator=(WorkArea&&) noexcept;

    WorkArea(const WorkArea&) = delete;
    WorkArea& operator=(const WorkArea&) = delete;

    bool Init(const LayoutData& layoutData);
    bool SetLayout(const LayoutData& layoutData);

    HMONITOR Monitor() const { return m_monitor; }
    RECT WorkAreaRect() const { return m_workAreaRect; }

    Layout* GetLayout() { return m_layout.get(); }
    const Layout* GetLayout() const { return m_layout.get(); }
    const LayoutData& GetLayoutData() const { return m_layoutData; }

    HWND GetWindow();

    bool Snap(HWND window, const ZoneIndexSet& zones);
    bool Unsnap(HWND window);

    void ShowZones(const ZoneIndexSet& highlightZones);
    void HideZones();
    void PreWarm();

    ZoneAssignmentStore* AssignmentStore() { return &m_assignments; }

private:
    bool EnsureWindow();
    void SetWorkAreaWindowAsTopmost(HWND draggedWindow);

    HINSTANCE m_hInstance = nullptr;
    HMONITOR m_monitor = nullptr;
    RECT m_workAreaRect{};
    const SettingsData& m_settings;
    LayoutData m_layoutData;
    std::unique_ptr<Layout> m_layout;
    HWND m_window = nullptr;
    std::unique_ptr<class ZonesOverlay> m_overlay;
    ZoneAssignmentStore m_assignments;
};
