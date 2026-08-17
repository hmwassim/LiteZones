#include "KeyboardSnap.h"

#include "LayoutAssignedWindows.h"
#include "Settings.h"
#include "WorkArea.h"
#include "WorkAreaManager.h"
#include "util.h"

KeyboardSnap::KeyboardSnap(WorkAreaManager& workAreaManager, const SettingsData& settings) :
    m_workAreaManager(workAreaManager), m_settings(settings)
{
}

bool KeyboardSnap::HandleKey(HWND window, DWORD vkCode)
{
    if (!window || !IsWindow(window))
    {
        return false;
    }

    if (vkCode >= '1' && vkCode <= '9')
    {
        return SnapByZoneNumber(window, static_cast<ZoneIndex>(vkCode - '1'));
    }
    if (vkCode == '0')
    {
        return SnapByZoneNumber(window, static_cast<ZoneIndex>(9));
    }

    if (vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_UP || vkCode == VK_DOWN)
    {
        return MoveByDirection(window, vkCode);
    }

    return false;
}

bool KeyboardSnap::SnapByZoneNumber(HWND window, ZoneIndex zoneIndex)
{
    WorkArea* workArea = WorkAreaForWindow(window);
    if (!workArea)
    {
        return false;
    }
    const Layout* layout = workArea->GetLayout();
    if (!layout || layout->Zones().find(zoneIndex) == layout->Zones().end())
    {
        return false;
    }

    if (workArea->Snap(window, { zoneIndex }))
    {
        UnsnapFromOtherWorkAreas(window, workArea);
        return true;
    }
    return false;
}

bool KeyboardSnap::MoveByDirection(HWND window, DWORD vkCode)
{
    if (m_settings.moveWindowsBasedOnPosition)
    {
        return MoveByDirectionAndPosition(window, vkCode);
    }
    return MoveByDirectionAndIndex(window, vkCode);
}

bool KeyboardSnap::MoveByDirectionAndIndex(HWND window, DWORD vkCode)
{
    WorkArea* workArea = WorkAreaForWindow(window);
    if (!workArea)
    {
        return false;
    }
    const Layout* layout = workArea->GetLayout();
    if (!layout)
    {
        return false;
    }
    const ZonesMap& zones = layout->Zones();
    if (zones.empty())
    {
        return false;
    }
    const int64_t numZones = static_cast<int64_t>(zones.size());

    const ZoneIndexSet zoneIndexes = workArea->LayoutWindows()->GetZoneIndexSetFromWindow(window);
    if (zoneIndexes.empty())
    {
        const ZoneIndex zone = (vkCode == VK_LEFT) ? static_cast<ZoneIndex>(numZones - 1) : 0;
        return workArea->Snap(window, { zone });
    }

    const ZoneIndex oldId = zoneIndexes[0];
    const bool atEdge = (vkCode == VK_LEFT && oldId == 0) || (vkCode == VK_RIGHT && oldId == numZones - 1);
    if (atEdge)
    {
        if (SnapOnAdjacentWorkArea(window, vkCode, workArea))
        {
            return true;
        }
        const ZoneIndex zone = (vkCode == VK_LEFT) ? static_cast<ZoneIndex>(numZones - 1) : 0;
        return workArea->Snap(window, { zone });
    }

    const ZoneIndex target = (vkCode == VK_LEFT) ? oldId - 1 : oldId + 1;
    return workArea->Snap(window, { target });
}

bool KeyboardSnap::MoveByDirectionAndPosition(HWND window, DWORD vkCode)
{
    WorkArea* workArea = WorkAreaForWindow(window);
    if (!workArea)
    {
        return false;
    }
    const Layout* layout = workArea->GetLayout();
    if (!layout)
    {
        return false;
    }
    const ZonesMap& zones = layout->Zones();
    if (zones.empty())
    {
        return false;
    }

    std::vector<bool> used(static_cast<size_t>(zones.size()), false);
    for (ZoneIndex id : workArea->LayoutWindows()->GetZoneIndexSetFromWindow(window))
    {
        if (id >= 0 && static_cast<size_t>(id) < used.size())
        {
            used[static_cast<size_t>(id)] = true;
        }
    }

    std::vector<RECT> freeZoneRects;
    std::vector<ZoneIndex> freeZoneIndices;
    for (const auto& [zoneId, zone] : zones)
    {
        if (zoneId >= 0 && static_cast<size_t>(zoneId) < used.size() && !used[static_cast<size_t>(zoneId)])
        {
            freeZoneRects.push_back(zone.GetZoneRect());
            freeZoneIndices.push_back(zoneId);
        }
    }

    RECT windowRect{};
    if (!GetWindowRect(window, &windowRect))
    {
        return false;
    }
    const RECT workAreaRect = workArea->WorkAreaRect();
    windowRect.top -= workAreaRect.top;
    windowRect.bottom -= workAreaRect.top;
    windowRect.left -= workAreaRect.left;
    windowRect.right -= workAreaRect.left;

    size_t result = Util::ChooseNextZoneByPosition(vkCode, windowRect, freeZoneRects);
    if (result < freeZoneRects.size())
    {
        return workArea->Snap(window, { freeZoneIndices[result] });
    }

    // Try again from the opposite edge, considering all zones as available.
    std::vector<RECT> zoneRects;
    std::vector<ZoneIndex> zoneIndices;
    zoneRects.reserve(zones.size());
    zoneIndices.reserve(zones.size());
    for (const auto& [zoneId, zone] : zones)
    {
        zoneRects.push_back(zone.GetZoneRect());
        zoneIndices.push_back(zoneId);
    }
    windowRect = Util::PrepareRectForCycling(windowRect, workAreaRect, vkCode);
    result = Util::ChooseNextZoneByPosition(vkCode, windowRect, zoneRects);
    if (result < zoneRects.size())
    {
        return workArea->Snap(window, { zoneIndices[result] });
    }
    return false;
}

bool KeyboardSnap::SnapOnAdjacentWorkArea(HWND window, DWORD vkCode, WorkArea* current)
{
    if (!m_settings.moveWindowAcrossMonitors)
    {
        return false;
    }

    std::vector<WorkArea>& workAreas = m_workAreaManager.WorkAreas();
    if (workAreas.size() < 2)
    {
        return false;
    }

    size_t index = workAreas.size();
    for (size_t i = 0; i < workAreas.size(); ++i)
    {
        if (&workAreas[i] == current)
        {
            index = i;
            break;
        }
    }
    if (index == workAreas.size())
    {
        return false;
    }

    const size_t adjacent = (vkCode == VK_RIGHT) ? (index + 1) % workAreas.size() : (index + workAreas.size() - 1) % workAreas.size();
    WorkArea* next = &workAreas[adjacent];
    const Layout* layout = next->GetLayout();
    if (!layout || layout->Zones().empty())
    {
        return false;
    }

    const ZoneIndex zone = (vkCode == VK_RIGHT) ? 0 : static_cast<ZoneIndex>(layout->Zones().size() - 1);
    if (next->Snap(window, { zone }))
    {
        // Snap rewrote the window's zone stamp; just drop the stale assignments.
        UnsnapFromOtherWorkAreas(window, next);
        return true;
    }
    return false;
}

void KeyboardSnap::UnsnapFromOtherWorkAreas(HWND window, WorkArea* keep)
{
    // Dismiss without calling WorkArea::Unsnap: that would remove the window's
    // zone stamp that the new snap just wrote.
    for (WorkArea& workArea : m_workAreaManager.WorkAreas())
    {
        if (&workArea != keep && workArea.LayoutWindows())
        {
            workArea.LayoutWindows()->Dismiss(window);
        }
    }
}

WorkArea* KeyboardSnap::WorkAreaForWindow(HWND window) const
{
    if (m_settings.spanZonesAcrossMonitors)
    {
        std::vector<WorkArea>& workAreas = m_workAreaManager.WorkAreas();
        return workAreas.empty() ? nullptr : &workAreas.front();
    }

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    WorkArea* workArea = m_workAreaManager.WorkAreaFor(monitor);
    if (workArea)
    {
        return workArea;
    }

    // Window not on a known monitor (e.g. just dragged off-screen): use primary.
    const HMONITOR primary = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    return m_workAreaManager.WorkAreaFor(primary);
}
