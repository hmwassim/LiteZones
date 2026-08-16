#pragma once

#include <windows.h>

#include <string>

namespace WindowUtils
{
    bool IsWindowMaximized(HWND window) noexcept;
    bool HasVisibleOwner(HWND window) noexcept;
    bool IsRoot(HWND window) noexcept;

    // Full path of the process owning the window (empty on failure).
    std::wstring GetProcessPath(HWND window) noexcept;

    // Stores the window's current size and origin as window properties (once).
    void SaveWindowSizeAndOrigin(HWND window) noexcept;
    void RestoreWindowSize(HWND window) noexcept;
    void RestoreWindowOrigin(HWND window) noexcept;

    // Moves/resizes a window so its client area matches the given screen-space rect.
    void SizeWindowToRect(HWND window, RECT rect, BOOL snapZone) noexcept;

    // Translates a screen-space rect into work-area coordinates for the monitor
    // it falls on (accounts for the taskbar offset).
    void ScreenToWorkAreaCoords(HWND window, RECT& rect) noexcept;

    RECT AdjustRectForSizeWindowToRect(HWND window, RECT rect, HWND windowOfRect) noexcept;

    // Makes the whole client area transparent via DWM blur-behind with an empty region.
    void MakeWindowTransparent(HWND window) noexcept;

    // True when the cursor shape indicates the user is resizing via a window border.
    bool IsCursorTypeIndicatingSizeEvent() noexcept;
}
