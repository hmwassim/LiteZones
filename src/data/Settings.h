#pragma once

#include <string>
#include <vector>

struct SettingsData
{
    bool shiftDrag = true;
    bool mouseSwitch = false;
    bool mouseMiddleClickSpanningMultipleZones = false;
    bool moveWindowAcrossMonitors = false;
    bool restoreSize = true;
    bool spanZonesAcrossMonitors = false;
    bool makeDraggedWindowTransparent = false;
    bool showZoneNumber = true;
    bool showZoneSize = false;
    int highlightOpacity = 50;

    std::wstring zoneColor = L"#AACDFF";
    std::wstring zoneBorderColor = L"#FFFFFF";
    std::wstring zoneHighlightColor = L"#FFFFFF";
    std::wstring zoneNumberColor = L"#000000";

    std::vector<std::wstring> excludedApps;

    // Internal bookkeeping (not a user-facing preference): whether the
    // one-time "LiteZones is running" tray balloon has already been shown on
    // this machine. Not surfaced in the Settings dialog.
    bool welcomeNotificationShown = false;
};

class Settings
{
public:
    static Settings& instance();

    // Loads settings.json (creating defaults if absent). Safe to call repeatedly.
    void Load();
    // Serializes current values to settings.json.
    void Save() const;

    SettingsData data;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};
