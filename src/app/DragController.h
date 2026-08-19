#pragma once

#include "HighlightedZones.h"

#include <windows.h>

class WorkArea;
class WorkAreaManager;
struct SettingsData;

class DragController
{
public:
    DragController(WorkAreaManager& workAreaManager, const SettingsData& settings);
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

    bool IsDragging() const { return m_draggingWindow != nullptr; }

private:
    bool IsDraggingEnabled() const;
    bool IsSelectManyZonesState() const;
    void UpdateShiftState();
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
    const SettingsData& m_settings;
    HWND m_draggingWindow = nullptr;
    WorkArea* m_currentWorkArea = nullptr;
    HighlightedZones m_highlightedZones;
    SavedWindowProperties m_windowProperties;
    bool m_snappingMode = false;
    bool m_shiftPressed = false;
    bool m_ctrlPressed = false;
    bool m_actualShiftPressed = false; // real keyboard shift state
    bool m_rightClickShift = false; // toggled by right-click, treated as shift
    bool m_middleMouse = false;
};
