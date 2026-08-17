#include "WorkArea.h"

#include "AppZoneHistory.h"
#include "Colors.h"
#include "Settings.h"
#include "WindowUtils.h"
#include "ZonesOverlay.h"

namespace
{
    constexpr wchar_t kToolWindowClassName[] = L"LiteZones_ZonesOverlay";

    LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    bool RegisterOverlayClass(HINSTANCE hInstance)
    {
        static bool registered = false;
        if (registered)
        {
            return true;
        }

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &OverlayWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kToolWindowClassName;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }

        registered = true;
        return true;
    }
}

WorkArea::WorkArea(HINSTANCE hInstance, HMONITOR monitor, const RECT& workAreaRect, const SettingsData& settings) :
    m_hInstance(hInstance),
    m_monitor(monitor),
    m_workAreaRect(workAreaRect),
    m_settings(settings)
{
}

WorkArea::WorkArea(WorkArea&& other) noexcept :
    m_hInstance(other.m_hInstance),
    m_monitor(other.m_monitor),
    m_workAreaRect(other.m_workAreaRect),
    m_settings(other.m_settings),
    m_layoutData(std::move(other.m_layoutData)),
    m_layout(std::move(other.m_layout)),
    m_window(other.m_window),
    m_overlay(std::move(other.m_overlay)),
    m_assignments(std::move(other.m_assignments))
{
    other.m_hInstance = nullptr;
    other.m_monitor = nullptr;
    other.m_workAreaRect = {};
    other.m_window = nullptr;
}

WorkArea& WorkArea::operator=(WorkArea&& other) noexcept
{
    if (this != &other)
    {
        // Destroy our current window/overlay before taking ownership of the other's.
        m_overlay.reset();
        if (m_window)
        {
            DestroyWindow(m_window);
        }

        m_hInstance = other.m_hInstance;
        m_monitor = other.m_monitor;
        m_workAreaRect = other.m_workAreaRect;
        m_layoutData = std::move(other.m_layoutData);
        m_layout = std::move(other.m_layout);
        m_window = other.m_window;
        m_overlay = std::move(other.m_overlay);
        m_assignments = std::move(other.m_assignments);

        other.m_hInstance = nullptr;
        other.m_monitor = nullptr;
        other.m_workAreaRect = {};
        other.m_window = nullptr;
    }
    return *this;
}

WorkArea::~WorkArea()
{
    // Tear down the renderer before destroying the window it draws into.
    m_overlay.reset();
    if (m_window)
    {
        DestroyWindow(m_window);
        m_window = nullptr;
    }
}

bool WorkArea::Init(const LayoutData& layoutData)
{
    auto layout = std::make_unique<Layout>(layoutData);
    if (!layout->Init(m_workAreaRect, m_monitor))
    {
        return false;
    }
    m_layoutData = layoutData;
    m_layout = std::move(layout);
    return true;
}

HWND WorkArea::GetWindow()
{
    if (!m_window)
    {
        EnsureWindow();
    }
    return m_window;
}

bool WorkArea::EnsureWindow()
{
    if (m_window)
    {
        return true;
    }
    if (!RegisterOverlayClass(m_hInstance))
    {
        return false;
    }

    m_window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                               kToolWindowClassName,
                               kToolWindowClassName,
                               WS_POPUP,
                               m_workAreaRect.left, m_workAreaRect.top,
                               m_workAreaRect.right - m_workAreaRect.left,
                               m_workAreaRect.bottom - m_workAreaRect.top,
                               nullptr, nullptr, m_hInstance, nullptr);
    if (!m_window)
    {
        return false;
    }

    // Transparent client area (DWM blur-behind with an empty region).
    WindowUtils::MakeWindowTransparent(m_window);

    // First display must use SW_SHOWNORMAL; hide it afterwards.
    ShowWindow(m_window, SW_SHOWNORMAL);
    ShowWindow(m_window, SW_HIDE);

    m_overlay = std::make_unique<ZonesOverlay>(m_window);
    return true;
}

bool WorkArea::Snap(HWND window, const ZoneIndexSet& zones)
{
    if (!m_layout || zones.empty())
    {
        return false;
    }

    if (!m_window)
    {
        EnsureWindow();
    }
    if (!m_window)
    {
        return false;
    }

    for (ZoneIndex zone : zones)
    {
        if (zone < 0 || static_cast<size_t>(zone) >= m_layout->Zones().size())
        {
            return false;
        }
    }

    m_assignments.Assign(window, zones);

    const RECT rect = m_layout->GetCombinedZonesRect(zones);
    const RECT adjustedRect = WindowUtils::AdjustRectForSizeWindowToRect(window, rect, m_window);
    WindowUtils::SaveWindowSizeAndOrigin(window);
    WindowUtils::SizeWindowToRect(window, adjustedRect, TRUE);

    const std::wstring processPath = WindowUtils::GetProcessPath(window);
    if (!processPath.empty())
    {
        AppZoneHistory::instance().SetAppLastZones(processPath, zones);
    }
    return true;
}

bool WorkArea::Unsnap(HWND window)
{
    if (!m_layout)
    {
        return false;
    }

    m_assignments.Dismiss(window);

    const std::wstring processPath = WindowUtils::GetProcessPath(window);
    if (!processPath.empty())
    {
        AppZoneHistory::instance().RemoveAppLastZone(processPath);
    }

    return true;
}

void WorkArea::ShowZones(const ZoneIndexSet& highlightZones)
{
    if (!m_layout)
    {
        return;
    }
    if (!m_overlay)
    {
        EnsureWindow();
    }
    if (m_layout && m_overlay)
    {
        SetWorkAreaWindowAsTopmost(nullptr);
        m_overlay->DrawActiveZoneSet(m_layout->Zones(), highlightZones, Colors::GetZoneColors(m_settings), m_settings.showZoneNumber);
        m_overlay->Show();
    }
}

void WorkArea::HideZones()
{
    if (m_overlay)
    {
        m_overlay->Hide();
    }
}

void WorkArea::PreWarm()
{
    EnsureWindow();
    if (m_overlay)
    {
        m_overlay->PreWarm();
    }
}

void WorkArea::SetWorkAreaWindowAsTopmost(HWND draggedWindow)
{
    if (!m_window)
    {
        return;
    }

    const HWND insertAfter = draggedWindow ? draggedWindow : HWND_TOPMOST;
    constexpr UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
    SetWindowPos(m_window, insertAfter, 0, 0, 0, 0, flags);
}
