#pragma once

#include <windows.h>

#include <functional>

class TrayService
{
public:
    TrayService() : m_autostart(IsAutostartEnabled()) {}
    ~TrayService();

    TrayService(const TrayService&) = delete;
    TrayService& operator=(const TrayService&) = delete;

    bool AddIcon(HWND hwnd, HINSTANCE hInstance);
    void RemoveIcon();
    void UpdateTip(HWND hwnd, bool snappingEnabled);
    void ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* message);

    // Builds and shows the tray context menu. Returns the selected action via callbacks.
    void ShowMenu(HWND hwnd, bool snappingEnabled);

    bool IsAutostartEnabled() const;
    void ToggleAutostart();

    void SetOnToggleSnapping(std::function<void()> cb) { m_onToggleSnapping = std::move(cb); }
    void SetOnCycleLayout(std::function<void()> cb) { m_onCycleLayout = std::move(cb); }
    void SetOnEditLayouts(std::function<void()> cb) { m_onEditLayouts = std::move(cb); }
    void SetOnReloadConfig(std::function<void()> cb) { m_onReloadConfig = std::move(cb); }
    void SetOnOpenFolder(std::function<void()> cb) { m_onOpenFolder = std::move(cb); }
    void SetOnSettings(std::function<void()> cb) { m_onSettings = std::move(cb); }
    void SetOnExit(std::function<void()> cb) { m_onExit = std::move(cb); }

private:
    bool m_autostart = false;

    std::function<void()> m_onToggleSnapping;
    std::function<void()> m_onCycleLayout;
    std::function<void()> m_onEditLayouts;
    std::function<void()> m_onReloadConfig;
    std::function<void()> m_onOpenFolder;
    std::function<void()> m_onSettings;
    std::function<void()> m_onExit;
};
