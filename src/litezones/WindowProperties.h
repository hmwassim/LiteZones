#pragma once

#include "Zone.h"

#include <windows.h>

#include <cstdint>

// Per-window property names used to remember snap state and pre-snap geometry.
namespace ZonedWindowProperties
{
    constexpr wchar_t PropertyRestoreOriginID[] = L"LiteZones_restoreOrigin";
    constexpr wchar_t PropertyRestoreSizeID[] = L"LiteZones_restoreSize";
    constexpr wchar_t PropertyZoneIndexSetPart1ID[] = L"LiteZones_zones";
    constexpr wchar_t PropertyZoneIndexSetPart2ID[] = L"LiteZones_zones_max128";
    constexpr wchar_t PropertyTabSortKeyWithinZoneID[] = L"LiteZones_TabSortKeyWithinZone";
}

// Stamps/reads arbitrary bytes into a per-window property (stored as a HANDLE).
void SetPropData(HWND window, const wchar_t* name, const void* data, size_t size) noexcept;
bool GetPropData(HWND window, const wchar_t* name, void* out, size_t size) noexcept;
uint64_t GetPropUint64(HWND window, const wchar_t* name) noexcept;

// A zone index set compressed into two 64-bit masks (supports up to 128 zones).
struct ZoneIndexSetBitmask
{
    uint64_t part1 = 0;
    uint64_t part2 = 0;

    static ZoneIndexSetBitmask FromIndexSet(const ZoneIndexSet& zones) noexcept;
    ZoneIndexSet ToIndexSet() const noexcept;
};

void StampZoneIndexProperty(HWND window, const ZoneIndexSetBitmask& mask) noexcept;
ZoneIndexSetBitmask RetrieveZoneIndexProperty(HWND window) noexcept;
void RemoveZoneIndexProperty(HWND window) noexcept;
