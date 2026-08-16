#pragma once

#include "Zone.h"

#include <string>
#include <unordered_map>

// Persists the last zone(s) each app was snapped into (app-zone-history.json).
// v1 keys history by process path only: layouts are uniform across monitors, so
// a single zone index set per app is sufficient (no monitor/virtual-desktop ids).
class AppZoneHistory
{
public:
    using HistoryMap = std::unordered_map<std::wstring, ZoneIndexSet>;

    static AppZoneHistory& instance();

    // Loads from %LOCALAPPDATA%\LiteZones\app-zone-history.json (or the override path).
    void LoadData();
    // Writes the current history to disk. Safe to call repeatedly.
    void SaveData() const;

    void Clear();

    ZoneIndexSet GetAppLastZoneIndexSet(const std::wstring& processPath) const;
    bool SetAppLastZones(const std::wstring& processPath, const ZoneIndexSet& zones);

    const HistoryMap& History() const { return m_history; }

    // Test hook: point LoadData/SaveData at a scratch file (empty = default).
    void SetPathOverride(const std::wstring& path) { m_pathOverride = path; }

private:
    AppZoneHistory() = default;
    AppZoneHistory(const AppZoneHistory&) = delete;
    AppZoneHistory& operator=(const AppZoneHistory&) = delete;

    std::wstring FilePath() const;
    std::unordered_map<std::wstring, ZoneIndexSet> m_history;
    std::wstring m_pathOverride;
};
