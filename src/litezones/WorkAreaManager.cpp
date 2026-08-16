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
        if (layout.type == FancyZonesDataTypes::ZoneSetLayoutType::Custom)
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

WorkAreaManager::WorkAreaManager(HINSTANCE hInstance) :
    m_hInstance(hInstance)
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

        // Preserve the previous WorkArea (and its snapped windows) when both the
        // monitor rect and the resolved layout are unchanged.
        auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return wa.Monitor() == monitor; });
        if (it != m_workAreas.end())
        {
            if (RectsEqual(it->WorkAreaRect(), rect) && (!forceRelayout || it->GetLayoutData() == layout))
            {
                updated.push_back(std::move(*it));
                continue;
            }
        }

        WorkArea wa(m_hInstance, monitor, rect);
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
