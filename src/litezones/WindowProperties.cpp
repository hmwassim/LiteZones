#include "WindowProperties.h"

#include <cstring>

void SetPropData(HWND window, const wchar_t* name, const void* data, size_t size) noexcept
{
    static_assert(sizeof(HANDLE) >= sizeof(uint64_t), "expected 64-bit HANDLE");
    if (!data || size > sizeof(HANDLE))
    {
        return;
    }
    HANDLE rawData{};
    std::memcpy(&rawData, data, size);
    SetPropW(window, name, rawData);
}

bool GetPropData(HWND window, const wchar_t* name, void* out, size_t size) noexcept
{
    static_assert(sizeof(HANDLE) >= sizeof(uint64_t), "expected 64-bit HANDLE");
    if (!out || size > sizeof(HANDLE))
    {
        return false;
    }
    const HANDLE rawData = GetPropW(window, name);
    if (!rawData)
    {
        return false;
    }
    std::memcpy(out, &rawData, size);
    return true;
}

uint64_t GetPropUint64(HWND window, const wchar_t* name) noexcept
{
    uint64_t value = 0;
    GetPropData(window, name, &value, sizeof(value));
    return value;
}

ZoneIndexSetBitmask ZoneIndexSetBitmask::FromIndexSet(const ZoneIndexSet& zones) noexcept
{
    ZoneIndexSetBitmask mask;
    for (ZoneIndex index : zones)
    {
        if (index < 0 || index >= 128)
        {
            continue;
        }
        if (index < 64)
        {
            mask.part1 |= uint64_t{ 1 } << static_cast<unsigned>(index);
        }
        else
        {
            mask.part2 |= uint64_t{ 1 } << static_cast<unsigned>(index - 64);
        }
    }
    return mask;
}

ZoneIndexSet ZoneIndexSetBitmask::ToIndexSet() const noexcept
{
    ZoneIndexSet zones;
    for (unsigned i = 0; i < 64; ++i)
    {
        if (part1 & (uint64_t{ 1 } << i))
        {
            zones.push_back(static_cast<ZoneIndex>(i));
        }
    }
    for (unsigned i = 0; i < 64; ++i)
    {
        if (part2 & (uint64_t{ 1 } << i))
        {
            zones.push_back(static_cast<ZoneIndex>(i + 64));
        }
    }
    return zones;
}

void StampZoneIndexProperty(HWND window, const ZoneIndexSetBitmask& mask) noexcept
{
    SetPropData(window, ZonedWindowProperties::PropertyZoneIndexSetPart1ID, &mask.part1, sizeof(mask.part1));
    SetPropData(window, ZonedWindowProperties::PropertyZoneIndexSetPart2ID, &mask.part2, sizeof(mask.part2));
}

ZoneIndexSetBitmask RetrieveZoneIndexProperty(HWND window) noexcept
{
    ZoneIndexSetBitmask mask;
    mask.part1 = GetPropUint64(window, ZonedWindowProperties::PropertyZoneIndexSetPart1ID);
    mask.part2 = GetPropUint64(window, ZonedWindowProperties::PropertyZoneIndexSetPart2ID);
    return mask;
}

void RemoveZoneIndexProperty(HWND window) noexcept
{
    RemovePropW(window, ZonedWindowProperties::PropertyZoneIndexSetPart1ID);
    RemovePropW(window, ZonedWindowProperties::PropertyZoneIndexSetPart2ID);
}
