#pragma once

#include "Zone.h"

#include <windows.h>

class WorkArea;
class WorkAreaManager;

// Ports WindowKeyboardSnap: keyboard snapping of the foreground window.
//  - Win+Ctrl+Alt+[0-9] snaps the window into the zone with that number.
//  - Win+Ctrl+Alt+arrows (and Win+arrows when overrideSnapHotkeys) move the
//    window to the adjacent zone; at an edge it cycles (or crosses monitors
//    when moveWindowAcrossMonitors is on).
class KeyboardSnap
{
public:
    explicit KeyboardSnap(WorkAreaManager& workAreaManager);

    // Returns true when the key was handled (a snap happened or the window moved).
    bool HandleKey(HWND window, DWORD vkCode);

private:
    bool SnapByZoneNumber(HWND window, ZoneIndex zoneIndex);
    bool MoveByDirection(HWND window, DWORD vkCode);
    bool MoveByDirectionAndIndex(HWND window, DWORD vkCode);
    bool MoveByDirectionAndPosition(HWND window, DWORD vkCode);
    bool SnapOnAdjacentWorkArea(HWND window, DWORD vkCode, WorkArea* current);
    void UnsnapFromOtherWorkAreas(HWND window, WorkArea* keep);
    WorkArea* WorkAreaForWindow(HWND window) const;

    WorkAreaManager& m_workAreaManager;
};
