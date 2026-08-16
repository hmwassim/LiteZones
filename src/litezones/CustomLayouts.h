#pragma once

#include "LayoutTypes.h"
#include "util.h"

#include <windows.h>

#include <map>
#include <optional>
#include <string>

// Persists user-defined grid/canvas layouts (custom-layouts.json) and mirrors
// them into LayoutEngine's rendering registry. Written by the editor (M5) and
// hot-reloaded by the runtime.
class CustomLayouts
{
public:
    using CustomLayoutMap = std::map<GUID, FancyZonesDataTypes::CustomLayoutData, Util::GuidLess>;

    static CustomLayouts& instance();

    // Loads from %LOCALAPPDATA%\LiteZones\custom-layouts.json (or the override path).
    // Malformed entries are dropped; the whole map is replaced on success.
    void LoadData();
    // Writes the current map to disk. Safe to call repeatedly.
    void SaveData() const;

    bool HasLayout(const GUID& uuid) const;
    // LayoutData for applying a custom layout (re-derives zoneCount/spacing/etc.
    // from the stored data). Returns std::nullopt when the uuid is unknown.
    std::optional<LayoutData> GetLayout(const GUID& uuid) const;
    const FancyZonesDataTypes::CustomLayoutData* GetCustomLayoutData(const GUID& uuid) const;

    // Inserts or replaces the layout and persists. Returns false on empty name.
    bool AddLayout(const GUID& uuid, const FancyZonesDataTypes::CustomLayoutData& data);
    // Removes the layout (also from the rendering registry) and persists.
    void DeleteLayout(const GUID& uuid);

    const CustomLayoutMap& AllLayouts() const { return m_layouts; }

    // Test hook: point LoadData/SaveData at a scratch file (empty = default).
    void SetPathOverride(const std::wstring& path) { m_pathOverride = path; }

private:
    CustomLayouts() = default;
    CustomLayouts(const CustomLayouts&) = delete;
    CustomLayouts& operator=(const CustomLayouts&) = delete;

    std::wstring FilePath() const;
    void SyncRegistry() const;

    CustomLayoutMap m_layouts;
    std::wstring m_pathOverride;
};
