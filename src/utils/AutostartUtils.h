#pragma once

#include <windows.h>

namespace Autostart
{
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValueName[] = L"LiteZones";

    inline bool IsEnabled()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return false;
        }

        wchar_t value[MAX_PATH]{};
        DWORD size = sizeof(value);
        const LONG result = RegQueryValueExW(key, kRunValueName, nullptr, nullptr, reinterpret_cast<BYTE*>(value), &size);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && value[0] != L'\0';
    }

    inline void SetEnabled(bool enabled)
    {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        {
            return;
        }

        if (enabled)
        {
            wchar_t path[MAX_PATH]{};
            if (GetModuleFileNameW(nullptr, path, MAX_PATH) > 0)
            {
                const DWORD size = static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t));
                RegSetValueExW(key, kRunValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(path), size);
            }
        }
        else
        {
            RegDeleteValueW(key, kRunValueName);
        }
        RegCloseKey(key);
    }
}
