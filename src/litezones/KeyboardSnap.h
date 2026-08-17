#pragma once

#include "Zone.h"

#include <windows.h>

class WorkArea;
class WorkAreaManager;
struct SettingsData;

class KeyboardSnap
{
public:
    KeyboardSnap(WorkAreaManager& workAreaManager, const SettingsData& settings);

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
    const SettingsData& m_settings;
};
