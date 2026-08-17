#include "MonitorManager.h"

#include "LayoutHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <tuple>

using std::min;
using std::max;

namespace
{
    constexpr UINT kDefaultDpi = 96;
}

namespace MonitorUtils
{
    std::vector<MonitorRect> GetAllMonitorWorkRects()
    {
        std::vector<MonitorRect> result;
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM param) -> BOOL {
            auto& output = *reinterpret_cast<std::vector<MonitorRect>*>(param);
            MONITORINFOEX mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(monitor, &mi))
            {
                output.push_back({ monitor, mi.rcWork });
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&result));
        return result;
    }

    void OrderMonitors(std::vector<MonitorRect>& monitorInfo)
    {
        const size_t nMonitors = monitorInfo.size();
        // blocking[i][j] - whether monitor i blocks monitor j in the ordering, i.e. monitor i should go before monitor j
        std::vector<std::vector<bool>> blocking(nMonitors, std::vector<bool>(nMonitors, false));

        // blockingCount[j] - the number of monitors which block monitor j
        std::vector<size_t> blockingCount(nMonitors, 0);

        for (size_t i = 0; i < nMonitors; i++)
        {
            const RECT rectI = monitorInfo[i].second;
            for (size_t j = 0; j < nMonitors; j++)
            {
                const RECT rectJ = monitorInfo[j].second;
                blocking[i][j] = rectI.top < rectJ.bottom && rectI.left < rectJ.right && i != j;
                if (blocking[i][j])
                {
                    blockingCount[j]++;
                }
            }
        }

        // used[i] - whether the sorting algorithm has used monitor i so far
        std::vector<bool> used(nMonitors, false);

        // the sorted sequence of monitors
        std::vector<MonitorRect> sortedMonitorInfo;

        for (size_t iteration = 0; iteration < nMonitors; iteration++)
        {
            // Indices of candidates to become the next monitor in the sequence
            std::vector<size_t> candidates;

            // First, find indices of all unblocked monitors
            for (size_t i = 0; i < nMonitors; i++)
            {
                if (blockingCount[i] == 0 && !used[i])
                {
                    candidates.push_back(i);
                }
            }

            // In the unlikely event that there are no unblocked monitors, declare all unused monitors as candidates.
            if (candidates.empty())
            {
                for (size_t i = 0; i < nMonitors; i++)
                {
                    if (!used[i])
                    {
                        candidates.push_back(i);
                    }
                }
            }

            // Pick the lexicographically smallest monitor as the next one
            size_t smallest = candidates[0];
            for (size_t j = 1; j < candidates.size(); j++)
            {
                const size_t current = candidates[j];

                // Compare (top, left) lexicographically
                if (std::tie(monitorInfo[current].second.top, monitorInfo[current].second.left) <
                    std::tie(monitorInfo[smallest].second.top, monitorInfo[smallest].second.left))
                {
                    smallest = current;
                }
            }

            used[smallest] = true;
            sortedMonitorInfo.push_back(monitorInfo[smallest]);
            for (size_t i = 0; i < nMonitors; i++)
            {
                if (blocking[smallest][i])
                {
                    blockingCount[i]--;
                }
            }
        }

        monitorInfo = std::move(sortedMonitorInfo);
    }

    RECT GetMonitorsCombinedRect(const std::vector<MonitorRect>& monitorRects)
    {
        bool empty = true;
        RECT result{ 0, 0, 0, 0 };

        for (const auto& [monitor, rect] : monitorRects)
        {
            (void)monitor;
            LayoutHelpers::ExtendBoundingRect(result, empty, rect);
        }

        return result;
    }

    UINT GetDpiForMonitor(HMONITOR monitor) noexcept
    {
        typedef BOOL(WINAPI* GetDpiForMonitorInternalFunc)(HMONITOR, UINT, UINT*, UINT*);

        UINT dpi{};
        HMODULE user32 = LoadLibraryW(L"user32.dll");
        if (user32)
        {
            if (auto func = reinterpret_cast<GetDpiForMonitorInternalFunc>(GetProcAddress(user32, "GetDpiForMonitorInternal")))
            {
                func(monitor, 0, &dpi, &dpi);
            }
            FreeLibrary(user32);
        }

        if (dpi == 0)
        {
            HDC hdc = GetDC(nullptr);
            if (hdc)
            {
                dpi = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
                ReleaseDC(nullptr, hdc);
            }
        }

        return (dpi == 0) ? kDefaultDpi : dpi;
    }

    Display GetDevice(HMONITOR monitor)
    {
        Display display;
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(monitor, &mi))
        {
            return display;
        }

        constexpr wchar_t kDisplayPrefix[] = L"DISPLAY";
        const wchar_t* numberStart = wcsstr(mi.szDevice, kDisplayPrefix);
        if (numberStart)
        {
            numberStart += wcslen(kDisplayPrefix);
            display.number = _wtoi(numberStart);
        }

        DISPLAY_DEVICEW dd{};
        dd.cb = sizeof(dd);
        if (EnumDisplayDevicesW(mi.szDevice, 0, &dd, EDD_GET_DEVICE_INTERFACE_NAME))
        {
            display.deviceId = dd.DeviceID;
            const size_t firstHash = display.deviceId.find(L'#');
            if (firstHash != std::wstring::npos)
            {
                const size_t secondHash = display.deviceId.find(L'#', firstHash + 1);
                if (secondHash != std::wstring::npos)
                {
                    const size_t thirdHash = display.deviceId.find(L'#', secondHash + 1);
                    display.instanceId = display.deviceId.substr(
                        secondHash + 1,
                        (thirdHash == std::wstring::npos) ? std::wstring::npos : thirdHash - secondHash - 1);
                }
            }
        }

        return display;
    }

    std::wstring GetDeviceKey(HMONITOR monitor)
    {
        const Display display = GetDevice(monitor);
        if (!display.deviceId.empty())
        {
            return display.instanceId.empty() ? display.deviceId : display.deviceId + L"|" + display.instanceId;
        }
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi))
        {
            return mi.szDevice;
        }
        return L"";
    }

    std::vector<MonitorRect> GetWorkAreas(bool span)
    {
        auto monitors = GetAllMonitorWorkRects();
        OrderMonitors(monitors);

        if (span && !monitors.empty())
        {
            const RECT combined = GetMonitorsCombinedRect(monitors);
            return { { monitors.front().first, combined } };
        }

        return monitors;
    }
}
