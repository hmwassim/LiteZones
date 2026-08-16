#include "DragController.h"

#include "Settings.h"
#include "WindowProcessing.h"
#include "WindowProperties.h"
#include "WindowUtils.h"
#include "WorkArea.h"
#include "WorkAreaManager.h"

DragController::DragController(WorkAreaManager& workAreaManager) :
    m_workAreaManager(workAreaManager)
{
}

DragController::~DragController()
{
    ResetWindowTransparency();
}

bool DragController::IsDraggingEnabled() const
{
    // Shift gates snapping unless the secondary mouse button is held instead.
    // The secondary mouse only participates when mouseSwitch is enabled.
    const bool secondaryHeld = Settings::instance().data.mouseSwitch ? m_secondaryMouse : false;
    const bool toggled = m_shiftPressed != secondaryHeld;
    return Settings::instance().data.shiftDrag ? toggled : !toggled;
}

bool DragController::IsSelectManyZonesState() const
{
    return m_ctrlPressed || m_middleMouse;
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
    if (!WindowProcessing::IsProcessableManually(window))
    {
        return;
    }

    m_draggingWindow = window;
    m_snappingMode = false;
    m_highlightedZones.Reset();

    POINT cursor{};
    GetCursorPos(&cursor);
    m_currentWorkArea = WorkAreaContaining(cursor, window);

    SwitchSnappingMode(IsDraggingEnabled());
}

void DragController::MoveSizeUpdate()
{
    if (!m_draggingWindow)
    {
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
        else if (Settings::instance().data.restoreSize)
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
}

void DragController::OnWindowDestroyed(HWND window)
{
    if (m_draggingWindow == window)
    {
        // End the drag WITHOUT snapping: the window is gone.
        SwitchSnappingMode(false);
        m_draggingWindow = nullptr;
        m_currentWorkArea = nullptr;
    }
}

void DragController::OnKeyStateChanged(UINT vk, bool pressed)
{
    if (vk == VK_SHIFT)
    {
        m_shiftPressed = pressed;
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
        m_secondaryMouse = down;
    }
    else if (button == VK_MBUTTON)
    {
        if (Settings::instance().data.mouseMiddleClickSpanningMultipleZones)
        {
            m_middleMouse = down;
        }
        else
        {
            m_secondaryMouse = down;
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
    if (Settings::instance().data.moveWindowAcrossMonitors)
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
    if (!Settings::instance().data.makeDraggedWindowTransparent)
    {
        return;
    }

    m_windowProperties.exstyle = GetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE);

    SetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE, m_windowProperties.exstyle | WS_EX_LAYERED);

    if (!GetLayeredWindowAttributes(m_draggingWindow, &m_windowProperties.colorKey, &m_windowProperties.alpha, &m_windowProperties.flags))
    {
        return;
    }

    if (SetLayeredWindowAttributes(m_draggingWindow, 0, (255 * 50) / 100, LWA_ALPHA))
    {
        m_windowProperties.transparencySet = true;
    }
}

void DragController::ResetWindowTransparency()
{
    if (!Settings::instance().data.makeDraggedWindowTransparent || !m_windowProperties.transparencySet)
    {
        return;
    }

    SetLayeredWindowAttributes(m_draggingWindow, m_windowProperties.colorKey, m_windowProperties.alpha, m_windowProperties.flags);
    SetWindowLongPtrW(m_draggingWindow, GWL_EXSTYLE, m_windowProperties.exstyle);
    m_windowProperties.transparencySet = false;
}
