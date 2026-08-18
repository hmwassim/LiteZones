#include "WorkAreaManager.h"

#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "MonitorManager.h"

#include <algorithm>

namespace
{
    bool RectsEqual(const RECT& a, const RECT& b)
    {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }

    bool RectContains(const RECT& rect, const POINT& pt)
    {
        return pt.x >= rect.left && pt.x < rect.right && pt.y >= rect.top && pt.y < rect.bottom;
    }
}

namespace LayoutResolver
{
    LayoutData Resolve(HMONITOR monitor, bool span, const LayoutData& defaultLayout)
    {
        if (span)
        {
            return defaultLayout;
        }

        const auto applied = AppliedLayouts::instance().GetDeviceLayout(MonitorUtils::GetDeviceKey(monitor));
        if (!applied.has_value())
        {
            return defaultLayout;
        }

        LayoutData layout = *applied;
        if (layout.type == LiteZonesTypes::ZoneSetLayoutType::Custom)
        {
            const auto custom = CustomLayouts::instance().GetLayout(layout.uuid);
            if (custom.has_value())
            {
                layout = *custom;
            }
            else
            {
                // The referenced custom layout no longer exists: fall back to default.
                return defaultLayout;
            }
        }
        return layout;
    }
}

WorkAreaManager::WorkAreaManager(HINSTANCE hInstance, const SettingsData& settings) :
    m_hInstance(hInstance), m_settings(settings)
{
}

void WorkAreaManager::Update(bool span, const LayoutData& defaultLayout, bool forceRelayout)
{
    const auto monitorWorkAreas = MonitorUtils::GetWorkAreas(span);

    std::vector<WorkArea> updated;
    updated.reserve(monitorWorkAreas.size());

    for (const auto& [monitor, rect] : monitorWorkAreas)
    {
        const LayoutData layout = LayoutResolver::Resolve(monitor, span, defaultLayout);

        auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return wa.Monitor() == monitor; });
        if (it != m_workAreas.end() && RectsEqual(it->WorkAreaRect(), rect))
        {
            if (!forceRelayout || it->GetLayoutData() == layout)
            {
                updated.push_back(std::move(*it));
                continue;
            }
            // Layout changed but monitor rect is the same: update in-place
            // to preserve the overlay window and D2D resources.
            it->SetLayout(layout);
            updated.push_back(std::move(*it));
            continue;
        }

        WorkArea wa(m_hInstance, monitor, rect, m_settings);
        wa.Init(layout);
        updated.push_back(std::move(wa));
    }

    m_workAreas = std::move(updated);
}

WorkArea* WorkAreaManager::WorkAreaFor(HMONITOR monitor)
{
    auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return wa.Monitor() == monitor; });
    return it == m_workAreas.end() ? nullptr : &(*it);
}

const WorkArea* WorkAreaManager::WorkAreaFor(HMONITOR monitor) const
{
    auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return wa.Monitor() == monitor; });
    return it == m_workAreas.end() ? nullptr : &(*it);
}

WorkArea* WorkAreaManager::WorkAreaForWindow(HWND window, bool span)
{
    if (span)
    {
        return m_workAreas.empty() ? nullptr : &m_workAreas.front();
    }

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    if (WorkArea* wa = WorkAreaFor(monitor))
    {
        return wa;
    }

    const HMONITOR primary = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    return WorkAreaFor(primary);
}

const WorkArea* WorkAreaManager::WorkAreaForWindow(HWND window, bool span) const
{
    if (span)
    {
        return m_workAreas.empty() ? nullptr : &m_workAreas.front();
    }

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    if (const WorkArea* wa = WorkAreaFor(monitor))
    {
        return wa;
    }

    const HMONITOR primary = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    return WorkAreaFor(primary);
}

WorkArea* WorkAreaManager::WorkAreaContainingPoint(POINT pt)
{
    auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return RectContains(wa.WorkAreaRect(), pt); });
    return it == m_workAreas.end() ? nullptr : &(*it);
}

const WorkArea* WorkAreaManager::WorkAreaContainingPoint(POINT pt) const
{
    auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return RectContains(wa.WorkAreaRect(), pt); });
    return it == m_workAreas.end() ? nullptr : &(*it);
}
