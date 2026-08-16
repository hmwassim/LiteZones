#include "util.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

namespace Util
{
    size_t ChooseNextZoneByPosition(DWORD vkCode, RECT windowRect, const std::vector<RECT>& zoneRects) noexcept
    {
        using Complex = std::complex<double>;
        const size_t invalidResult = zoneRects.size();
        constexpr double inf = 1e100;
        constexpr double eccentricity = 2.0;

        auto rectCenter = [](RECT rect) {
            return Complex{
                0.5 * rect.left + 0.5 * rect.right,
                0.5 * rect.top + 0.5 * rect.bottom
            };
        };

        auto distance = [](Complex arrowDirection, Complex zoneDirection) {
            double result = inf;

            const double scalarProduct = (arrowDirection * std::conj(zoneDirection)).real();
            if (scalarProduct <= 0.0)
            {
                return inf;
            }

            // No need to divide by abs(arrowDirection): it is 1 for unit arrows.
            const double cosAngle = scalarProduct / std::abs(zoneDirection);
            const double tanAngle = std::abs(std::tan(std::acos(cosAngle)));
            if (tanAngle > 10)
            {
                // The angle is too wide.
                return inf;
            }

            // Intersect an ellipse with the given eccentricity, major axis along arrowDirection.
            const double intersectY = 2 * eccentricity / (1.0 + eccentricity * eccentricity * tanAngle * tanAngle);
            const double distanceEstimate = scalarProduct / intersectY;
            if (std::isfinite(distanceEstimate))
            {
                result = distanceEstimate;
            }

            return result;
        };

        std::vector<std::pair<size_t, Complex>> candidateCenters;
        candidateCenters.reserve(zoneRects.size());
        for (size_t i = 0; i < zoneRects.size(); i++)
        {
            Complex center = rectCenter(zoneRects[i]);

            // Offset the zone slightly to differentiate overlapping zones.
            center += 0.001 * (i + 1);

            candidateCenters.emplace_back(i, center);
        }

        Complex directionVector{};
        const Complex windowCenter = rectCenter(windowRect);

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
            const double dist = distance(directionVector, zoneCenter - windowCenter);
            if (dist < smallestDistance)
            {
                smallestDistance = dist;
                closestIdx = zoneIdx;
            }
        }

        return closestIdx;
    }
}
