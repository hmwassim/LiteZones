#include "ZoneAssignmentStore.h"
#include "WindowProperties.h"

void ZoneAssignmentStore::Assign(HWND window, const ZoneIndexSet& zones)
{
    m_layoutWindows.Assign(window, zones);
    StampZoneIndexProperty(window, ZoneIndexSetBitmask::FromIndexSet(zones));
}

void ZoneAssignmentStore::Dismiss(HWND window)
{
    m_layoutWindows.Dismiss(window);
    RemoveZoneIndexProperty(window);
}

ZoneIndexSet ZoneAssignmentStore::GetZoneIndexSet(HWND window) const
{
    return m_layoutWindows.GetZoneIndexSetFromWindow(window);
}

bool ZoneAssignmentStore::IsZoneEmpty(ZoneIndex zone) const
{
    return m_layoutWindows.IsZoneEmpty(zone);
}
