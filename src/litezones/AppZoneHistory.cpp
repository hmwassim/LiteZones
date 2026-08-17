#include "AppZoneHistory.h"

#include "Paths.h"
#include "json.h"

namespace
{
    const std::wstring kAppZoneHistory = L"app-zone-history";
    const std::wstring kAppPath = L"app-path";
    const std::wstring kZoneIndexSet = L"zone-index-set";
}

AppZoneHistory& AppZoneHistory::instance()
{
    static AppZoneHistory history;
    return history;
}

std::wstring AppZoneHistory::FilePath() const
{
    return m_pathOverride.empty() ? Paths::AppZoneHistoryFile() : m_pathOverride;
}

void AppZoneHistory::LoadData()
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

    HistoryMap fresh;
    const Json& entries = root.At(kAppZoneHistory);
    if (entries.type() == Json::Type::Array)
    {
        for (size_t i = 0; i < entries.Size(); ++i)
        {
            const Json& entry = entries.At(i);
            if (entry.type() != Json::Type::Object)
            {
                continue;
            }

            const std::wstring appPath = entry.At(kAppPath).AsString();
            if (appPath.empty())
            {
                continue;
            }

            ZoneIndexSet zones;
            const Json& set = entry.At(kZoneIndexSet);
            if (set.type() == Json::Type::Array)
            {
                for (size_t j = 0; j < set.Size(); ++j)
                {
                    zones.push_back(static_cast<ZoneIndex>(set.At(j).AsNumber()));
                }
            }
            if (!zones.empty())
            {
                fresh[appPath] = std::move(zones);
            }
        }
    }

    m_history = std::move(fresh);
}

void AppZoneHistory::SaveData() const
{
    Json root = Json::MakeObject();
    Json entries = Json::MakeArray();
    for (const auto& [appPath, zones] : m_history)
    {
        Json entry = Json::MakeObject();
        entry.Set(kAppPath, appPath);

        Json set = Json::MakeArray();
        for (ZoneIndex zone : zones)
        {
            set.Push(Json::MakeNumber(static_cast<double>(zone)));
        }
        entry.Set(kZoneIndexSet, set);
        entries.Push(entry);
    }
    root.Set(kAppZoneHistory, entries);

    Paths::WriteTextFile(FilePath(), root.SerializeIndented(), /*crlf=*/false);
}

bool AppZoneHistory::SetAppLastZones(const std::wstring& processPath, const ZoneIndexSet& zones)
{
    if (processPath.empty() || zones.empty())
    {
        return false;
    }
    m_history[processPath] = zones;
    m_dirty = true;
    return true;
}

void AppZoneHistory::RemoveAppLastZone(const std::wstring& processPath)
{
    if (m_history.erase(processPath) > 0)
    {
        m_dirty = true;
    }
}

void AppZoneHistory::FlushIfDirty() const
{
    if (!m_dirty)
    {
        return;
    }
    m_dirty = false;
    SaveData();
}
