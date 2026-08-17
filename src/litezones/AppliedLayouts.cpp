#include "AppliedLayouts.h"

#include "Paths.h"
#include "json.h"
#include "GuidUtils.h"

namespace
{
    const std::wstring kAppliedLayouts = L"applied-layouts";
    const std::wstring kDevice = L"device";
    const std::wstring kMonitor = L"monitor";
    const std::wstring kMonitorInstance = L"monitor-instance";
    const std::wstring kMonitorNumber = L"monitor-number";
    const std::wstring kSerialNumber = L"serial-number";
    const std::wstring kVirtualDesktop = L"virtual-desktop";
    const std::wstring kAppliedLayout = L"applied-layout";
    const std::wstring kUuid = L"uuid";
    const std::wstring kType = L"type";
    const std::wstring kShowSpacing = L"show-spacing";
    const std::wstring kSpacing = L"spacing";
    const std::wstring kZoneCount = L"zone-count";
    const std::wstring kSensitivityRadius = L"sensitivity-radius";

    // The stable lookup key for a monitor: the EnumDisplayDevices interface
    // string (\\?\DISPLAY#id#instance#...). Entries are matched on this exact
    // string plus the instance component (which it already contains).
    std::wstring DeviceKeyFromJson(const Json& device)
    {
        const std::wstring monitor = device.At(kMonitor).AsString();
        const std::wstring instance = device.At(kMonitorInstance).AsString();
        if (monitor.empty())
        {
            return L"";
        }
        if (instance.empty())
        {
            return monitor;
        }
        return monitor + L"|" + instance;
    }

    bool ParseLayout(const Json& json, LayoutData& out)
    {
        GUID uuid = GUID_NULL;
        if (!Util::GuidFromString(json.At(kUuid).AsString(), uuid))
        {
            return false;
        }

        out.uuid = uuid;
        out.type = Util::TypeFromString(json.At(kType).AsString());
        out.showSpacing = json.At(kShowSpacing).AsBool(DefaultValues::ShowSpacing);
        out.spacing = static_cast<int>(json.At(kSpacing).AsNumber(DefaultValues::Spacing));
        out.zoneCount = static_cast<int>(json.At(kZoneCount).AsNumber(DefaultValues::ZoneCount));
        out.sensitivityRadius = static_cast<int>(json.At(kSensitivityRadius).AsNumber(DefaultValues::SensitivityRadius));
        return true;
    }

    Json SerializeLayout(const LayoutData& data)
    {
        Json json = Json::MakeObject();
        json.Set(kUuid, Util::GuidToString(data.uuid));
        json.Set(kType, Util::TypeToString(data.type));
        json.Set(kShowSpacing, data.showSpacing);
        json.Set(kSpacing, static_cast<double>(data.spacing));
        json.Set(kZoneCount, static_cast<double>(data.zoneCount));
        json.Set(kSensitivityRadius, static_cast<double>(data.sensitivityRadius));
        return json;
    }
}

AppliedLayouts& AppliedLayouts::instance()
{
    static AppliedLayouts applied;
    return applied;
}

std::wstring AppliedLayouts::FilePath() const
{
    return m_pathOverride.empty() ? Paths::AppliedLayoutsFile() : m_pathOverride;
}

void AppliedLayouts::LoadData()
{
    std::wstring text;
    if (!Paths::ReadTextFile(FilePath(), text))
    {
        return;
    }

    Json root;
    if (!Json::Parse(text, root))
    {
        return;
    }

    AppliedLayoutMap fresh;
    const Json& entries = root.At(kAppliedLayouts);
    if (entries.type() == Json::Type::Array)
    {
        for (size_t i = 0; i < entries.Size(); ++i)
        {
            const Json& entry = entries.At(i);
            if (entry.type() != Json::Type::Object)
            {
                continue;
            }

            const Json& device = entry.At(kDevice);
            if (device.type() != Json::Type::Object)
            {
                continue;
            }
            const std::wstring key = DeviceKeyFromJson(device);
            if (key.empty())
            {
                continue;
            }

            const Json& layoutJson = entry.At(kAppliedLayout);
            if (layoutJson.type() != Json::Type::Object)
            {
                continue;
            }
            LayoutData layout;
            if (!ParseLayout(layoutJson, layout))
            {
                continue;
            }

            if (fresh.find(key) == fresh.end())
            {
                fresh[key] = layout;
            }
        }
    }

    m_layouts = std::move(fresh);
}

void AppliedLayouts::SaveData() const
{
    Json root = Json::MakeObject();
    Json entries = Json::MakeArray();
    for (const auto& [key, data] : m_layouts)
    {
        Json device = Json::MakeObject();
        // The key is "<interface-string>|<instance>"; store both parts. When no
        // instance was recorded the key is just the interface string.
        std::wstring monitor = key;
        std::wstring instance;
        const size_t separator = key.find(L'|');
        if (separator != std::wstring::npos)
        {
            monitor = key.substr(0, separator);
            instance = key.substr(separator + 1);
        }
        device.Set(kMonitor, monitor);
        device.Set(kMonitorInstance, instance);
        device.Set(kMonitorNumber, 0.0);
        device.Set(kSerialNumber, L"");
        device.Set(kVirtualDesktop, L"00000000-0000-0000-0000-000000000000");

        Json entry = Json::MakeObject();
        entry.Set(kDevice, device);
        entry.Set(kAppliedLayout, SerializeLayout(data));
        entries.Push(entry);
    }
    root.Set(kAppliedLayouts, entries);

    Paths::WriteTextFile(FilePath(), root.SerializeIndented(), /*crlf=*/false);
}

std::optional<LayoutData> AppliedLayouts::GetDeviceLayout(const std::wstring& deviceKey) const
{
    const auto it = m_layouts.find(deviceKey);
    if (it == m_layouts.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void AppliedLayouts::ApplyLayout(const std::wstring& deviceKey, const LayoutData& layout)
{
    m_layouts[deviceKey] = layout;
}
