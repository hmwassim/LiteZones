#include "WindowUtils.h"

#include "WindowProperties.h"

#include <dwmapi.h>
#include <string>

namespace WindowUtils
{
    bool IsWindowMaximized(HWND window) noexcept
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        return GetWindowPlacement(window, &placement) && placement.showCmd == SW_SHOWMAXIMIZED;
    }

    bool HasVisibleOwner(HWND window) noexcept
    {
        const HWND owner = GetWindow(window, GW_OWNER);
        if (!owner)
        {
            return false;
        }
        if (!IsWindowVisible(owner))
        {
            return false;
        }
        RECT rect{};
        if (!GetWindowRect(owner, &rect))
        {
            return true;
        }
        return rect.top != rect.bottom && rect.left != rect.right;
    }

    bool IsRoot(HWND window) noexcept
    {
        return GetAncestor(window, GA_ROOT) == window;
    }

    std::wstring GetProcessPath(HWND window) noexcept
    {
        std::wstring path(MAX_PATH, L'\0');
        const DWORD written = GetWindowModuleFileNameW(window, path.data(), static_cast<DWORD>(path.size()));
        if (written == 0)
        {
            return std::wstring();
        }
        path.resize(written);
        return path;
    }

    void ScreenToWorkAreaCoords(HWND window, RECT& rect) noexcept
    {
        (void)window;

        HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!GetMonitorInfoW(monitor, &monitorInfo))
        {
            return;
        }

        LONG xOffset = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
        LONG yOffset = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;

        RECT referenceRect = rect;
        referenceRect.left -= xOffset;
        referenceRect.right -= xOffset;
        referenceRect.top -= yOffset;
        referenceRect.bottom -= yOffset;

        // Re-resolve the monitor from the work-area-space rect: this fixes zones
        // that sit between two monitors with the taskbar on the left.
        monitor = MonitorFromRect(&referenceRect, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(monitor, &monitorInfo))
        {
            return;
        }

        xOffset = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
        yOffset = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;

        rect.left -= xOffset;
        rect.right -= xOffset;
        rect.top -= yOffset;
        rect.bottom -= yOffset;
    }

    void SizeWindowToRect(HWND window, RECT rect, BOOL snapZone) noexcept
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        GetWindowPlacement(window, &placement);

        // Wait while SW_SHOWMINIMIZED would be removed from the window (Issue #1685).
        for (int i = 0; i < 5 && placement.showCmd == SW_SHOWMINIMIZED; ++i)
        {
            Sleep(100);
            GetWindowPlacement(window, &placement);
        }

        BOOL maximizeLater = FALSE;
        if (IsWindowVisible(window))
        {
            if (!snapZone && placement.showCmd == SW_SHOWMAXIMIZED)
            {
                maximizeLater = TRUE;
            }

            // Do not restore minimized windows. We change their placement though so they restore to the correct zone.
            if (placement.showCmd != SW_SHOWMINIMIZED && placement.showCmd != SW_MINIMIZE)
            {
                if (placement.showCmd == SW_SHOWMAXIMIZED)
                {
                    placement.flags &= ~WPF_RESTORETOMAXIMIZED;
                }
                placement.showCmd = SW_RESTORE;
            }
        }
        else
        {
            placement.showCmd = SW_HIDE;
        }

        ScreenToWorkAreaCoords(window, rect);

        placement.rcNormalPosition = rect;
        placement.flags |= WPF_ASYNCWINDOWPLACEMENT;

        SetWindowPlacement(window, &placement);

        // make sure the window is moved to the correct monitor before maximize
        if (maximizeLater)
        {
            placement.showCmd = SW_SHOWMAXIMIZED;
        }

        // Do it again, allowing Windows to resize the window and set the correct scaling (Issue #365).
        SetWindowPlacement(window, &placement);
    }

    void SaveWindowSizeAndOrigin(HWND window) noexcept
    {
        if (GetPropW(window, ZonedWindowProperties::PropertyRestoreSizeID))
        {
            // Size already saved.
            return;
        }

        RECT rect{};
        if (!GetWindowRect(window, &rect))
        {
            return;
        }

        const int size[2] = { rect.right - rect.left, rect.bottom - rect.top };
        const int origin[2] = { rect.left, rect.top };

        SetPropData(window, ZonedWindowProperties::PropertyRestoreSizeID, size, sizeof(size));
        SetPropData(window, ZonedWindowProperties::PropertyRestoreOriginID, origin, sizeof(origin));
    }

    void RestoreWindowSize(HWND window) noexcept
    {
        int size[2]{};
        if (!GetPropData(window, ZonedWindowProperties::PropertyRestoreSizeID, size, sizeof(size)))
        {
            return;
        }

        RECT rect{};
        if (GetWindowRect(window, &rect))
        {
            rect.right = rect.left + size[0];
            rect.bottom = rect.top + size[1];
            SizeWindowToRect(window, rect, FALSE);
        }

        RemovePropW(window, ZonedWindowProperties::PropertyRestoreSizeID);
    }

    void RestoreWindowOrigin(HWND window) noexcept
    {
        int origin[2]{};
        if (!GetPropData(window, ZonedWindowProperties::PropertyRestoreOriginID, origin, sizeof(origin)))
        {
            return;
        }

        RECT rect{};
        if (GetWindowRect(window, &rect))
        {
            const int xOffset = origin[0] - rect.left;
            const int yOffset = origin[1] - rect.top;
            rect.left += xOffset;
            rect.right += xOffset;
            rect.top += yOffset;
            rect.bottom += yOffset;
            SizeWindowToRect(window, rect, FALSE);
        }

        RemovePropW(window, ZonedWindowProperties::PropertyRestoreOriginID);
    }

    RECT AdjustRectForSizeWindowToRect(HWND window, RECT rect, HWND windowOfRect) noexcept
    {
        RECT newWindowRect = rect;

        RECT windowRect{};
        GetWindowRect(window, &windowRect);

        // Take care of borders (skip when windowOfRect is not initialized, e.g. in unit tests).
        if (windowOfRect)
        {
            RECT frameRect{};
            if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frameRect, sizeof(frameRect))))
            {
                const LONG leftMargin = frameRect.left - windowRect.left;
                const LONG rightMargin = frameRect.right - windowRect.right;
                const LONG bottomMargin = frameRect.bottom - windowRect.bottom;
                newWindowRect.left -= leftMargin;
                newWindowRect.right -= rightMargin;
                newWindowRect.bottom -= bottomMargin;
            }
        }

        // Take care of windows that cannot be resized.
        if ((GetWindowLong(window, GWL_STYLE) & WS_SIZEBOX) == 0)
        {
            newWindowRect.right = newWindowRect.left + (windowRect.right - windowRect.left);
            newWindowRect.bottom = newWindowRect.top + (windowRect.bottom - windowRect.top);
        }

        // Convert to screen coordinates.
        if (windowOfRect)
        {
            MapWindowPoints(windowOfRect, nullptr, reinterpret_cast<LPPOINT>(&newWindowRect), 2);
        }

        return newWindowRect;
    }

    void MakeWindowTransparent(HWND window) noexcept
    {
        const int pos = -GetSystemMetrics(SM_CXVIRTUALSCREEN) - 8;
        const HRGN region = CreateRectRgn(pos, 0, pos + 1, 1);
        if (!region)
        {
            return;
        }

        DWM_BLURBEHIND blurBehind{};
        blurBehind.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        blurBehind.fEnable = TRUE;
        blurBehind.hRgnBlur = region;
        blurBehind.fTransitionOnMaximized = FALSE;
        DwmEnableBlurBehindWindow(window, &blurBehind);

        DeleteObject(region);
    }

    bool IsCursorTypeIndicatingSizeEvent() noexcept
    {
        CURSORINFO cursorInfo{};
        cursorInfo.cbSize = sizeof(cursorInfo);
        if (!GetCursorInfo(&cursorInfo))
        {
            return false;
        }

        const LPCWSTR sizeCursors[] = { IDC_SIZENS, IDC_SIZEWE, IDC_SIZENESW, IDC_SIZENWSE };
        for (LPCWSTR cursor : sizeCursors)
        {
            if (LoadCursorW(nullptr, cursor) == cursorInfo.hCursor)
            {
                return true;
            }
        }
        return false;
    }
}
