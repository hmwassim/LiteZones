#pragma once

#include <string>
#include <vector>

struct SettingsData
{
    bool shiftDrag = true;
    bool mouseSwitch = false;
    bool mouseMiddleClickSpanningMultipleZones = false;
    bool moveWindowAcrossMonitors = false;
    bool moveWindowsBasedOnPosition = false;
    bool snapToAppZoneOnOpen = false;
    bool overrideSnapHotkeys = true;
    bool restoreSize = true;
    bool openWindowOnActiveMonitor = false;
    bool spanZonesAcrossMonitors = false;
    bool makeDraggedWindowTransparent = false;
    bool showZoneNumber = true;
    int highlightOpacity = 50;

    std::wstring zoneColor = L"#AACDFF";
    std::wstring zoneBorderColor = L"#FFFFFF";
    std::wstring zoneHighlightColor = L"#AACDFF";
    std::wstring zoneNumberColor = L"#000000";
    std::wstring overlappingZonesAlgorithm = L"closestCenter";

    std::vector<std::wstring> excludedApps;
};

class Settings
{
public:
    static Settings& instance();

    // Loads settings.json (creating defaults if absent). Safe to call repeatedly.
    void Load();
    // Serializes current values to settings.json.
    void Save() const;
    // True once Load() has succeeded (or written defaults).
    bool loaded() const;

    SettingsData data;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool m_loaded = false;
};
