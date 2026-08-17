#pragma once

#include "LayoutTypes.h"

#include <windows.h>

#include <string>

// GUID parsing/formatting and layout-type string helpers.
namespace Util
{
    struct GuidLess
    {
        bool operator()(const GUID& lhs, const GUID& rhs) const noexcept
        {
            return memcmp(&lhs, &rhs, sizeof(GUID)) < 0;
        }
    };

    // Strict "8-4-4-4-12" hex GUID parsing (no braces). Returns false on malformed input.
    bool GuidFromString(const std::wstring& str, GUID& out) noexcept;

    // Canonical uppercase "8-4-4-4-12" form.
    std::wstring GuidToString(const GUID& guid) noexcept;

    std::wstring TypeToString(FancyZonesDataTypes::ZoneSetLayoutType type) noexcept;
    FancyZonesDataTypes::ZoneSetLayoutType TypeFromString(const std::wstring& value) noexcept;
}
