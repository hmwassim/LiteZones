#pragma once

#include "LayoutTypes.h"

#include <windows.h>

#include <map>
#include <optional>
#include <string>

// Persists which layout applies to each monitor (applied-layouts.json). v1 keys
// monitors by the stable EnumDisplayDevices interface string (no WMI serial, no
// virtual-desktop keying); span-across-monitors mode ignores these assignments.
class AppliedLayouts
{
public:
    using AppliedLayoutMap = std::map<std::wstring, LayoutData>;

    static AppliedLayouts& instance();

    // Loads from %LOCALAPPDATA%\LiteZones\applied-layouts.json (or the override path).
    void LoadData();
    // Writes the current map to disk. Safe to call repeatedly.
    void SaveData() const;

    std::optional<LayoutData> GetDeviceLayout(const std::wstring& deviceKey) const;
    void ApplyLayout(const std::wstring& deviceKey, const LayoutData& layout);

    const AppliedLayoutMap& AllLayouts() const { return m_layouts; }

    // Test hook: point LoadData/SaveData at a scratch file (empty = default).
    void SetPathOverride(const std::wstring& path) { m_pathOverride = path; }

private:
    AppliedLayouts() = default;
    AppliedLayouts(const AppliedLayouts&) = delete;
    AppliedLayouts& operator=(const AppliedLayouts&) = delete;

    std::wstring FilePath() const;

    AppliedLayoutMap m_layouts;
    std::wstring m_pathOverride;
};
