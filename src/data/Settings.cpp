#include "Settings.h"

#include "Paths.h"
#include "json.h"

#include <windows.h>

namespace
{
    const std::wstring kKeyShiftDrag = L"shiftDrag";
    const std::wstring kKeyMouseSwitch = L"mouseSwitch";
    const std::wstring kKeyMouseMiddleClickSpanningMultipleZones = L"mouseMiddleClickSpanningMultipleZones";
    const std::wstring kKeyMoveWindowAcrossMonitors = L"moveWindowAcrossMonitors";
    const std::wstring kKeyRestoreSize = L"restoreSize";
    const std::wstring kKeySpanZonesAcrossMonitors = L"spanZonesAcrossMonitors";
    const std::wstring kKeyMakeDraggedWindowTransparent = L"makeDraggedWindowTransparent";
    const std::wstring kKeyShowZoneNumber = L"showZoneNumber";
    const std::wstring kKeyShowZoneSize = L"showZoneSize";
    const std::wstring kKeyHighlightOpacity = L"highlightOpacity";
    const std::wstring kKeyZoneColor = L"zoneColor";
    const std::wstring kKeyZoneBorderColor = L"zoneBorderColor";
    const std::wstring kKeyZoneHighlightColor = L"zoneHighlightColor";
    const std::wstring kKeyZoneNumberColor = L"zoneNumberColor";
    const std::wstring kKeyExcludedApps = L"excludedApps";

    constexpr int kMinOpacity = 0;
    constexpr int kMaxOpacity = 100;

    int ClampOpacity(int value)
    {
        return value < kMinOpacity ? kMinOpacity : (value > kMaxOpacity ? kMaxOpacity : value);
    }
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

void Settings::Load()
{
    const std::wstring file = Paths::SettingsFile();
    if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        // First run: write defaults so the user has a reference file.
        Save();
        return;
    }

    std::wstring text;
    if (!Paths::ReadTextFile(file, text))
    {
        // Transient read failure (file being written by an editor). Keep previous values.
        return;
    }

    Json root;
    if (!Json::Parse(text, root))
    {
        // Corrupt file. Keep previous values; do not clobber the user's file.
        return;
    }

    SettingsData fresh;
    if (root.type() == Json::Type::Object)
    {
        fresh.shiftDrag = root.At(kKeyShiftDrag).AsBool(fresh.shiftDrag);
        fresh.mouseSwitch = root.At(kKeyMouseSwitch).AsBool(fresh.mouseSwitch);
        fresh.mouseMiddleClickSpanningMultipleZones = root.At(kKeyMouseMiddleClickSpanningMultipleZones).AsBool(fresh.mouseMiddleClickSpanningMultipleZones);
        fresh.moveWindowAcrossMonitors = root.At(kKeyMoveWindowAcrossMonitors).AsBool(fresh.moveWindowAcrossMonitors);
        fresh.restoreSize = root.At(kKeyRestoreSize).AsBool(fresh.restoreSize);
        fresh.spanZonesAcrossMonitors = root.At(kKeySpanZonesAcrossMonitors).AsBool(fresh.spanZonesAcrossMonitors);
        fresh.makeDraggedWindowTransparent = root.At(kKeyMakeDraggedWindowTransparent).AsBool(fresh.makeDraggedWindowTransparent);
        fresh.showZoneNumber = root.At(kKeyShowZoneNumber).AsBool(fresh.showZoneNumber);
        fresh.showZoneSize = root.At(kKeyShowZoneSize).AsBool(fresh.showZoneSize);
        fresh.highlightOpacity = ClampOpacity(static_cast<int>(root.At(kKeyHighlightOpacity).AsNumber(fresh.highlightOpacity)));

        fresh.zoneColor = root.At(kKeyZoneColor).AsString(fresh.zoneColor);
        fresh.zoneBorderColor = root.At(kKeyZoneBorderColor).AsString(fresh.zoneBorderColor);
        fresh.zoneHighlightColor = root.At(kKeyZoneHighlightColor).AsString(fresh.zoneHighlightColor);
        fresh.zoneNumberColor = root.At(kKeyZoneNumberColor).AsString(fresh.zoneNumberColor);

        const Json& excluded = root.At(kKeyExcludedApps);
        if (excluded.type() == Json::Type::Array)
        {
            fresh.excludedApps.clear();
            for (size_t i = 0; i < excluded.Size(); ++i)
            {
                const std::wstring app = excluded.At(i).AsString();
                if (!app.empty())
                {
                    fresh.excludedApps.push_back(app);
                }
            }
        }
    }

    data = std::move(fresh);
}

void Settings::Save() const
{
    if (!Paths::EnsureConfigDir())
    {
        return;
    }

    Json root = Json::MakeObject();
    root.Set(kKeyShiftDrag, data.shiftDrag);
    root.Set(kKeyMouseSwitch, data.mouseSwitch);
    root.Set(kKeyMouseMiddleClickSpanningMultipleZones, data.mouseMiddleClickSpanningMultipleZones);
    root.Set(kKeyMoveWindowAcrossMonitors, data.moveWindowAcrossMonitors);
    root.Set(kKeyRestoreSize, data.restoreSize);
    root.Set(kKeySpanZonesAcrossMonitors, data.spanZonesAcrossMonitors);
    root.Set(kKeyMakeDraggedWindowTransparent, data.makeDraggedWindowTransparent);
    root.Set(kKeyShowZoneNumber, data.showZoneNumber);
    root.Set(kKeyShowZoneSize, data.showZoneSize);
    root.Set(kKeyHighlightOpacity, static_cast<double>(data.highlightOpacity));
    root.Set(kKeyZoneColor, data.zoneColor);
    root.Set(kKeyZoneBorderColor, data.zoneBorderColor);
    root.Set(kKeyZoneHighlightColor, data.zoneHighlightColor);
    root.Set(kKeyZoneNumberColor, data.zoneNumberColor);

    Json apps = Json::MakeArray();
    for (const auto& app : data.excludedApps)
    {
        apps.Push(Json::MakeString(app));
    }
    root.Set(kKeyExcludedApps, apps);

    Paths::WriteTextFile(Paths::SettingsFile(), root.SerializeIndented(), /*crlf=*/true);
}
