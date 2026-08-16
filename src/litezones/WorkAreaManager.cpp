#include "WorkAreaManager.h"

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

WorkAreaManager::WorkAreaManager(HINSTANCE hInstance) :
    m_hInstance(hInstance)
{
}

void WorkAreaManager::Update(bool span, const LayoutData& defaultLayout)
{
    const auto monitorWorkAreas = MonitorUtils::GetWorkAreas(span);

    std::vector<WorkArea> updated;
    updated.reserve(monitorWorkAreas.size());

    // Preserve the previous WorkArea (and its layout) for monitors that are still present.
    for (const auto& [monitor, rect] : monitorWorkAreas)
    {
        auto it = std::find_if(m_workAreas.begin(), m_workAreas.end(), [&](const WorkArea& wa) { return wa.Monitor() == monitor; });
        if (it != m_workAreas.end())
        {
            if (RectsEqual(it->WorkAreaRect(), rect))
            {
                updated.push_back(std::move(*it));
                continue;
            }
            // Rect changed (resolution/scale): rebuild with the same layout data.
            WorkArea wa(m_hInstance, monitor, rect);
            wa.Init(it->GetLayoutData());
            updated.push_back(std::move(wa));
            continue;
        }

        WorkArea wa(m_hInstance, monitor, rect);
        wa.Init(defaultLayout);
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
