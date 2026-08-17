#pragma once

#include "LayoutAssignedWindows.h"
#include "LayoutEngine.h"

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

    // Builds (or rebuilds) the layout from the given settings. Returns false if the layout is invalid.
    bool Init(const LayoutData& layoutData);

    HMONITOR Monitor() const { return m_monitor; }
    RECT WorkAreaRect() const { return m_workAreaRect; }

    Layout* GetLayout() { return m_layout.get(); }
    const Layout* GetLayout() const { return m_layout.get(); }
    const LayoutData& GetLayoutData() const { return m_layoutData; }

    // Lazily-created tool window covering the work area; the overlay draws into it.
    HWND GetWindow();

    // Snaps the window into the given zones. Returns false when nothing was snapped.
    bool Snap(HWND window, const ZoneIndexSet& zones);
    bool Unsnap(HWND window);

    void ShowZones(const ZoneIndexSet& highlightZones);
    void HideZones();

    LayoutAssignedWindows* LayoutWindows() { return m_layoutWindows.get(); }

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
    std::unique_ptr<LayoutAssignedWindows> m_layoutWindows;
};
