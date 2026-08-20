#include "WindowUtils.h"

#include "WindowProperties.h"

#include <dwmapi.h>
#include <cstdlib>
#include <string>

namespace
{
    constexpr size_t kMaxPathLength = 32768;
    constexpr int kMaxMinimizedWaitRetries = 5;
    constexpr DWORD kRetrySleepMs = 100;
    constexpr int kMaxPlacementRetries = 10;
    constexpr int kPlacementTolerancePx = 4;

    bool PlacementLanded(HWND window, const RECT& screenTarget) noexcept
    {
        RECT current{};
        if (!GetWindowRect(window, &current))
        {
            return true; // window is gone; nothing left to correct
        }
        return std::abs(current.left - screenTarget.left) <= kPlacementTolerancePx &&
               std::abs(current.top - screenTarget.top) <= kPlacementTolerancePx &&
               std::abs(current.right - current.left - (screenTarget.right - screenTarget.left)) <= kPlacementTolerancePx &&
               std::abs(current.bottom - current.top - (screenTarget.bottom - screenTarget.top)) <= kPlacementTolerancePx;
    }

    // Context for the background placement-retry thread. Heap-allocated and
    // owned by the thread proc so the caller (a hook-callback thread) never
    // blocks waiting on it.
    struct PlacementRetryContext
    {
        HWND window;
        RECT rect;
        RECT screenTarget;
        BOOL maximizeLater;
    };

    DWORD WINAPI RetryPlacementThreadProc(LPVOID param)
    {
        PlacementRetryContext* ctx = static_cast<PlacementRetryContext*>(param);

        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);

        // Some apps (notably Chromium-based browsers) keep overriding the
        // placement while their move loop is still finalizing, so keep
        // re-applying until the window actually lands on the target (or give
        // up after ~1 second). This runs off the hook-callback thread so a
        // stubborn app can't stall input processing for the whole session.
        for (int attempt = 0; attempt < kMaxPlacementRetries; ++attempt)
        {
            if (PlacementLanded(ctx->window, ctx->screenTarget))
            {
                break;
            }

            Sleep(kRetrySleepMs);
            GetWindowPlacement(ctx->window, &placement);
            if (ctx->maximizeLater || placement.showCmd == SW_SHOWMAXIMIZED)
            {
                placement.showCmd = SW_SHOWMAXIMIZED;
            }
            else
            {
                placement.showCmd = SW_RESTORE;
            }
            placement.rcNormalPosition = ctx->rect;
            placement.flags &= ~WPF_ASYNCWINDOWPLACEMENT;
            SetWindowPlacement(ctx->window, &placement);
        }

        delete ctx;
        return 0;
    }
}

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
        DWORD processId = 0;
        if (!GetWindowThreadProcessId(window, &processId) || processId == 0)
        {
            return std::wstring();
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!process)
        {
            return std::wstring();
        }

        std::wstring path(kMaxPathLength, L'\0');
        DWORD written = static_cast<DWORD>(path.size());
        if (!QueryFullProcessImageNameW(process, 0, path.data(), &written))
        {
            CloseHandle(process);
            return std::wstring();
        }
        path.resize(written);

        CloseHandle(process);
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
        for (int i = 0; i < kMaxMinimizedWaitRetries && placement.showCmd == SW_SHOWMINIMIZED; ++i)
        {
            Sleep(kRetrySleepMs);
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

        const RECT screenTarget = rect;

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

        // Verify the placement landed; if not, hand the retry-and-verify loop
        // off to a short-lived background thread instead of blocking here.
        // This function runs on the hook-callback thread (the same thread
        // that pumps WH_MOUSE_LL/WH_KEYBOARD_LL and the WinEvent hooks), so
        // looping with Sleep() in-line would stall input processing for up
        // to ~1 second whenever a stubborn app (e.g. Chromium) needs nudging.
        if (!PlacementLanded(window, screenTarget))
        {
            PlacementRetryContext* ctx = new PlacementRetryContext{ window, rect, screenTarget, maximizeLater };
            HANDLE thread = CreateThread(nullptr, 0, &RetryPlacementThreadProc, ctx, 0, nullptr);
            if (thread)
            {
                CloseHandle(thread);
            }
            else
            {
                delete ctx;
            }
        }
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
