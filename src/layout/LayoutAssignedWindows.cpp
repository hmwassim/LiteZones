#include "LayoutAssignedWindows.h"

#include <algorithm>

void LayoutAssignedWindows::Assign(HWND window, const ZoneIndexSet& zones)
{
    Dismiss(window);

    m_windowIndexSet[window] = zones;
    m_windowsByIndexSets[zones].push_back(window);
}

void LayoutAssignedWindows::Dismiss(HWND window)
{
    const auto it = m_windowIndexSet.find(window);
    if (it == m_windowIndexSet.end())
    {
        return;
    }

    const ZoneIndexSet zones = it->second;
    m_windowIndexSet.erase(it);

    const auto indexSetIt = m_windowsByIndexSets.find(zones);
    if (indexSetIt != m_windowsByIndexSets.end())
    {
        auto& windows = indexSetIt->second;
        windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
        if (windows.empty())
        {
            m_windowsByIndexSets.erase(indexSetIt);
        }
    }
}

bool LayoutAssignedWindows::IsZoneEmpty(ZoneIndex zone) const
{
    for (const auto& [zones, windows] : m_windowsByIndexSets)
    {
        if (!windows.empty())
        {
            for (ZoneIndex z : zones)
            {
                if (z == zone)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

ZoneIndexSet LayoutAssignedWindows::GetZoneIndexSetFromWindow(HWND window) const
{
    const auto it = m_windowIndexSet.find(window);
    if (it == m_windowIndexSet.end())
    {
        return {};
    }
    return it->second;
}
