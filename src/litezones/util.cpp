#include "util.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

namespace
{
    int HexValue(wchar_t c)
    {
        if (c >= L'0' && c <= L'9')
        {
            return c - L'0';
        }
        if (c >= L'a' && c <= L'f')
        {
            return c - L'a' + 10;
        }
        if (c >= L'A' && c <= L'F')
        {
            return c - L'A' + 10;
        }
        return -1;
    }
}

namespace Util
{
    bool GuidFromString(const std::wstring& str, GUID& out) noexcept
    {
        if (str.size() != 36)
        {
            return false;
        }

        unsigned char bytes[16]{};
        size_t hexCount = 0;
        for (size_t i = 0; i < str.size(); ++i)
        {
            const wchar_t c = str[i];
            if (c == L'-')
            {
                continue;
            }
            const int value = HexValue(c);
            if (value < 0)
            {
                return false;
            }
            if (hexCount % 2 == 0)
            {
                bytes[hexCount / 2] = static_cast<unsigned char>(value << 4);
            }
            else
            {
                bytes[hexCount / 2] = static_cast<unsigned char>(bytes[hexCount / 2] | value);
            }
            ++hexCount;
        }
        if (hexCount != 32)
        {
            return false;
        }

        out.Data1 = (static_cast<unsigned long>(bytes[0]) << 24) |
                    (static_cast<unsigned long>(bytes[1]) << 16) |
                    (static_cast<unsigned long>(bytes[2]) << 8) |
                    static_cast<unsigned long>(bytes[3]);
        out.Data2 = static_cast<unsigned short>((bytes[4] << 8) | bytes[5]);
        out.Data3 = static_cast<unsigned short>((bytes[6] << 8) | bytes[7]);
        for (size_t i = 0; i < 8; ++i)
        {
            out.Data4[i] = bytes[8 + i];
        }
        return true;
    }

    std::wstring GuidToString(const GUID& guid) noexcept
    {
        wchar_t buffer[64]{};
        wsprintfW(buffer, L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                  guid.Data1, guid.Data2, guid.Data3,
                  guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                  guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return buffer;
    }

    std::wstring TypeToString(FancyZonesDataTypes::ZoneSetLayoutType type) noexcept
    {
        switch (type)
        {
        case FancyZonesDataTypes::ZoneSetLayoutType::Blank:
            return L"blank";
        case FancyZonesDataTypes::ZoneSetLayoutType::Focus:
            return L"focus";
        case FancyZonesDataTypes::ZoneSetLayoutType::Rows:
            return L"rows";
        case FancyZonesDataTypes::ZoneSetLayoutType::Columns:
            return L"columns";
        case FancyZonesDataTypes::ZoneSetLayoutType::Grid:
            return L"grid";
        case FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid:
            return L"priority-grid";
        case FancyZonesDataTypes::ZoneSetLayoutType::Custom:
            return L"custom";
        }
        return L"blank";
    }

    FancyZonesDataTypes::ZoneSetLayoutType TypeFromString(const std::wstring& value) noexcept
    {
        if (value == L"focus")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::Focus;
        }
        if (value == L"rows")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::Rows;
        }
        if (value == L"columns")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::Columns;
        }
        if (value == L"grid")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::Grid;
        }
        if (value == L"priority-grid")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid;
        }
        if (value == L"custom")
        {
            return FancyZonesDataTypes::ZoneSetLayoutType::Custom;
        }
        return FancyZonesDataTypes::ZoneSetLayoutType::Blank;
    }

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
