#include "DragController.h"

#include "Settings.h"

namespace
{
    constexpr BYTE kDragTransparencyAlpha = 128; // 50% of 255
}
#include "WindowProcessing.h"
#include "WindowProperties.h"
#include "WindowUtils.h"
#include "WorkArea.h"
#include "WorkAreaManager.h"

DragController::DragController(WorkAreaManager& workAreaManager, const SettingsData& settings) :
    m_workAreaManager(workAreaManager), m_settings(settings)
{
}

DragController::~DragController()
{
    ResetWindowTransparency();
}

bool DragController::IsDraggingEnabled() const
{
    // Right-click toggles m_shiftPressed directly (via UpdateShiftState), making
    // it behave identically to holding the shift key — same flag, same code path.
    return m_settings.shiftDrag ? m_shiftPressed : !m_shiftPressed;
}

bool DragController::IsSelectManyZonesState() const
{
    return m_ctrlPressed || m_middleMouse;
}

void DragController::UpdateShiftState()
{
    // XOR: real shift and right-click-shift cancel each other out, so only
    // one needs to be active for snapping to toggle.
    m_shiftPressed = m_actualShiftPressed != m_rightClickShift;
}

void DragController::MoveSizeStart(HWND window)
{
    if (m_draggingWindow)
    {
        return;
    }
    if (WindowUtils::IsCursorTypeIndicatingSizeEvent())
    {
        return;
    }
    if (!WindowProcessing::IsProcessableManually(window, m_settings))
    {
        return;
    }

    m_draggingWindow = window;
    m_snappingMode = false;
    m_rightClickShift = false;
    UpdateShiftState();
    m_highlightedZones.Reset();

    POINT cursor{};
    GetCursorPos(&cursor);
    m_currentWorkArea = WorkAreaContaining(cursor, window);

    if (m_currentWorkArea)
    {
        m_currentWorkArea->Unsnap(window);
    }

    SwitchSnappingMode(IsDraggingEnabled());
}

void DragController::MoveSizeUpdate()
{
    if (!m_draggingWindow)
    {
        return;
    }

    // Safety net for the early-return in MoveSizeEnd(): that guard suppresses
    // a MOVESIZEEND WinEvent that fires while the left button still looks
    // held, on the assumption a real MOVESIZEEND will follow once the button
    // is actually released. If Windows never sends that follow-up event,
    // the drag would otherwise stay stuck forever. This re-checks the
    // physical button state directly, so the drag ends itself on the very
    // next update once the button is genuinely up.
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
    {
        MoveSizeEnd();
        return;
    }

    POINT cursor{};
    GetCursorPos(&cursor);

    const bool isSnapping = IsDraggingEnabled();
    SwitchSnappingMode(isSnapping);

    if (isSnapping)
    {
        WorkArea* workArea = WorkAreaContaining(cursor, m_draggingWindow);
        if (!workArea)
        {
            workArea = m_currentWorkArea;
        }

        if (workArea && workArea != m_currentWorkArea)
        {
            m_highlightedZones.Reset();
            if (m_currentWorkArea)
            {
                m_currentWorkArea->HideZones();
            }
            m_currentWorkArea = workArea;
            m_currentWorkArea->ShowZones(m_highlightedZones.Zones());
        }

        if (m_currentWorkArea)
        {
            POINT clientPoint = cursor;
            MapWindowPoints(nullptr, m_currentWorkArea->GetWindow(), &clientPoint, 1);

            const bool redraw = m_highlightedZones.Update(m_currentWorkArea->GetLayout(), clientPoint, IsSelectManyZonesState());
            if (redraw)
            {
                m_currentWorkArea->ShowZones(m_highlightedZones.Zones());
            }
        }
    }
}

void DragController::MoveSizeEnd()
{
    // A secondary button press (right/middle-click) during a drag can cause
    // Windows to fire EVENT_SYSTEM_MOVESIZEEND even though the user is still
    // dragging. Ignore it if the left button is still held; MoveSizeUpdate()
    // has a matching safety net in case no further WinEvent ever arrives.
    if (m_draggingWindow && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
    {
        return;
    }

    if (m_draggingWindow)
    {
        if (m_snappingMode)
        {
            if (!WindowUtils::IsWindowMaximized(m_draggingWindow))
            {
                if (m_currentWorkArea)
                {
                    // Snaps only when at least one zone is highlighted.
                    m_currentWorkArea->Snap(m_draggingWindow, m_highlightedZones.Zones());
                }
            }
        }
        else if (m_settings.restoreSize)
        {
            if (WindowUtils::IsCursorTypeIndicatingSizeEvent())
            {
                // The user was resizing via the border, not dragging: forget the saved size.
                RemovePropW(m_draggingWindow, ZonedWindowProperties::PropertyRestoreSizeID);
            }
            else if (!WindowUtils::IsWindowMaximized(m_draggingWindow))
            {
                WindowUtils::RestoreWindowSize(m_draggingWindow);
            }
        }
    }

    SwitchSnappingMode(false);
    m_draggingWindow = nullptr;
    m_currentWorkArea = nullptr;
    m_rightClickShift = false;
    UpdateShiftState();
}

void DragController::OnWindowDestroyed(HWND window)
{
    if (m_draggingWindow == window)
    {
        // End the drag WITHOUT snapping: the window is gone.
        SwitchSnappingMode(false);
        m_draggingWindow = nullptr;
        m_currentWorkArea = nullptr;
        m_rightClickShift = false;
        UpdateShiftState();
    }
}

void DragController::OnKeyStateChanged(UINT vk, bool pressed)
{
    if (vk == VK_SHIFT)
    {
        m_actualShiftPressed = pressed;
        UpdateShiftState();
    }
    else if (vk == VK_CONTROL)
    {
        m_ctrlPressed = pressed;
    }
    else
    {
        return;
    }

    if (m_draggingWindow)
    {
        MoveSizeUpdate();
    }
}

void DragController::OnMouseButtonChanged(UINT button, bool down)
{
    if (button == VK_RBUTTON)
    {
        if (m_settings.mouseSwitch && down && m_draggingWindow)
        {
            // Toggle right-click-shift: treated identically to the real shift key
            // via UpdateShiftState(), so the right-click path is the same code path.
            m_rightClickShift = !m_rightClickShift;
            UpdateShiftState();
        }
    }
    else if (button == VK_MBUTTON)
    {
        if (m_settings.mouseMiddleClickSpanningMultipleZones)
        {
            m_middleMouse = down;
        }
        else if (m_settings.mouseSwitch && down && m_draggingWindow)
        {
            m_rightClickShift = !m_rightClickShift;
            UpdateShiftState();
        }
    }
    else
    {
        return;
    }

    if (m_draggingWindow)
    {
        MoveSizeUpdate();
    }
}

void DragController::SwitchSnappingMode(bool isSnapping)
{
    if (!m_snappingMode && isSnapping)
    {
        // Turn on
        m_highlightedZones.Reset();
        SetWindowTransparency();

        if (m_currentWorkArea)
        {
            m_currentWorkArea->ShowZones(m_highlightedZones.Zones());
            m_currentWorkArea->Unsnap(m_draggingWindow);
        }
    }
    else if (m_snappingMode && !isSnapping)
    {
        // Turn off
        ResetWindowTransparency();
        m_highlightedZones.Reset();

        if (m_currentWorkArea)
        {
            m_currentWorkArea->HideZones();
        }
    }

    m_snappingMode = isSnapping;
}

WorkArea* DragController::WorkAreaContaining(const POINT& cursor, HWND window) const
{
    if (m_settings.moveWindowAcrossMonitors)
    {
        return m_workAreaManager.WorkAreaContainingPoint(cursor);
    }

    RECT windowRect{};
    GetWindowRect(window, &windowRect);
    const HMONITOR monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONULL);
    WorkArea* workArea = m_workAreaManager.WorkAreaFor(monitor);
    if (!workArea)
    {
        workArea = m_workAreaManager.WorkAreaContainingPoint(cursor);
    }
    return workArea;
}

void DragController::SetWindowTransparency()
{
    if (!m_settings.makeDraggedWindowTransparent)
    {
        return;
    }

    m_windowProperties.exstyle = GetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE);
    const bool wasLayered = (m_windowProperties.exstyle & WS_EX_LAYERED) != 0;

    SetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE, m_windowProperties.exstyle | WS_EX_LAYERED);

    // GetLayeredWindowAttributes only succeeds for windows that were already
    // layered before we just added WS_EX_LAYERED above (per MSDN), which is
    // false for almost every normal window. In that (common) case there are
    // no "previous" layered attributes to preserve, so fall back to values
    // that make ResetWindowTransparency() simply strip WS_EX_LAYERED again
    // instead of bailing out early and leaking the extended style change.
    if (!wasLayered || !GetLayeredWindowAttributes(m_draggingWindow, &m_windowProperties.colorKey, &m_windowProperties.alpha, &m_windowProperties.flags))
    {
        m_windowProperties.colorKey = 0;
        m_windowProperties.alpha = 255;
        m_windowProperties.flags = LWA_ALPHA;
    }

    if (SetLayeredWindowAttributes(m_draggingWindow, 0, kDragTransparencyAlpha, LWA_ALPHA))
    {
        m_windowProperties.transparencySet = true;
    }
}

void DragController::ResetWindowTransparency()
{
    if (!m_settings.makeDraggedWindowTransparent || !m_windowProperties.transparencySet)
    {
        return;
    }

    SetLayeredWindowAttributes(m_draggingWindow, m_windowProperties.colorKey, m_windowProperties.alpha, m_windowProperties.flags);
    SetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE, m_windowProperties.exstyle);
    m_windowProperties.transparencySet = false;
}
