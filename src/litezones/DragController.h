#pragma once

#include "HighlightedZones.h"

#include <windows.h>

class WorkArea;
class WorkAreaManager;

// Drives the drag-to-snap interaction for a single dragged window: decides when
// snapping is active (Shift / secondary-mouse toggle), tracks the highlighted
// zones, shows/hides the overlay, and snaps or restores on drop.
class DragController
{
public:
    explicit DragController(WorkAreaManager& workAreaManager);
    ~DragController();

    DragController(const DragController&) = delete;
    DragController& operator=(const DragController&) = delete;

    // WinEvent-driven entry points.
    void MoveSizeStart(HWND window);
    void MoveSizeUpdate();
    void MoveSizeEnd();
    void OnWindowDestroyed(HWND window);

    // Keyboard / mouse state changes while dragging.
    void OnKeyStateChanged(UINT vk, bool pressed);
    void OnMouseButtonChanged(UINT button, bool down);

    HWND DraggedWindow() const { return m_draggingWindow; }
    bool IsDragging() const { return m_draggingWindow != nullptr; }

private:
    bool IsDraggingEnabled() const;
    bool IsSelectManyZonesState() const;
    void SwitchSnappingMode(bool isSnapping);
    WorkArea* WorkAreaContaining(const POINT& cursor, HWND window) const;
    void SetWindowTransparency();
    void ResetWindowTransparency();

    struct SavedWindowProperties
    {
        LONG_PTR exstyle = 0;
        DWORD colorKey = 0;
        BYTE alpha = 0;
        DWORD flags = 0;
        bool transparencySet = false;
    };

    WorkAreaManager& m_workAreaManager;
    HWND m_draggingWindow = nullptr;
    WorkArea* m_currentWorkArea = nullptr;
    HighlightedZones m_highlightedZones;
    SavedWindowProperties m_windowProperties;
    bool m_snappingMode = false;
    bool m_shiftPressed = false;
    bool m_ctrlPressed = false;
    bool m_secondaryMouse = false;
    bool m_middleMouse = false;
};
