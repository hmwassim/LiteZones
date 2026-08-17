#include "ZoneNavigation.h"

#include <cmath>
#include <utility>

namespace Util
{
    namespace
    {
        struct Vec2
        {
            double x = 0.0;
            double y = 0.0;

            Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
            Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
            Vec2 operator*(double s) const { return { x * s, y * s }; }

            double dot(Vec2 o) const { return x * o.x + y * o.y; }
            double magnitude() const { return std::sqrt(x * x + y * y); }
        };

        Vec2 rectCenter(RECT rect)
        {
            return { 0.5 * rect.left + 0.5 * rect.right,
                     0.5 * rect.top + 0.5 * rect.bottom };
        }

        double directionalDistance(Vec2 arrowDirection, Vec2 zoneDirection)
        {
            constexpr double inf = 1e100;
            constexpr double eccentricity = 2.0;
            constexpr double kMaxAngleTan = 10.0;

            const double scalarProduct = arrowDirection.dot(zoneDirection);
            if (scalarProduct <= 0.0)
            {
                return inf;
            }

            const double cosAngle = scalarProduct / zoneDirection.magnitude();
            const double tanAngle = std::abs(std::tan(std::acos(cosAngle)));
            if (tanAngle > kMaxAngleTan)
            {
                return inf;
            }

            const double intersectY = 2 * eccentricity / (1.0 + eccentricity * eccentricity * tanAngle * tanAngle);
            const double distanceEstimate = scalarProduct / intersectY;
            return std::isfinite(distanceEstimate) ? distanceEstimate : inf;
        }
    }

    size_t ChooseNextZoneByPosition(DWORD vkCode, RECT windowRect, const std::vector<RECT>& zoneRects) noexcept
    {
        const size_t invalidResult = zoneRects.size();
        constexpr double inf = 1e100;
        constexpr double kZoneOverlapOffset = 0.001;

        std::vector<std::pair<size_t, Vec2>> candidateCenters;
        candidateCenters.reserve(zoneRects.size());
        for (size_t i = 0; i < zoneRects.size(); i++)
        {
            Vec2 center = rectCenter(zoneRects[i]);
            center = center + Vec2{ kZoneOverlapOffset * (i + 1), kZoneOverlapOffset * (i + 1) };
            candidateCenters.emplace_back(i, center);
        }

        Vec2 directionVector{};
        const Vec2 windowCenter = rectCenter(windowRect);

        switch (vkCode)
        {
        case VK_UP:
            directionVector = { 0.0, -1.0 };
            break;
        case VK_DOWN:
            directionVector = { 0.0, 1.0 };
            break;
        case VK_LEFT:
            directionVector = { -1.0, 0.0 };
            break;
        case VK_RIGHT:
            directionVector = { 1.0, 0.0 };
            break;
        default:
            return invalidResult;
        }

        size_t closestIdx = invalidResult;
        double smallestDistance = inf;

        for (const auto& [zoneIdx, zoneCenter] : candidateCenters)
        {
            const double dist = directionalDistance(directionVector, zoneCenter - windowCenter);
            if (dist < smallestDistance)
            {
                smallestDistance = dist;
                closestIdx = zoneIdx;
            }
        }

        return closestIdx;
    }
}
