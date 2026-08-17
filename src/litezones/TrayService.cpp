#include "TrayService.h"
#include "resource.h"
#include <shellapi.h>

namespace
{
    constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    constexpr UINT kTrayIconId = 1;
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValueName[] = L"LiteZones";
    constexpr wchar_t kWindowTitle[] = L"LiteZones";

    constexpr UINT kMenuToggleSnapping = 40001;
    constexpr UINT kMenuCycleLayout = 40002;
    constexpr UINT kMenuReloadConfig = 40003;
    constexpr UINT kMenuOpenFolder = 40004;
    constexpr UINT kMenuAutostart = 40005;
    constexpr UINT kMenuExit = 40006;
    constexpr UINT kMenuEditLayouts = 40007;
}

TrayService::~TrayService()
{
    RemoveIcon();
}

bool TrayService::AddIcon(HWND hwnd, HINSTANCE hInstance)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMessage;
    nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wcscpy_s(nid.szTip, kWindowTitle);
    return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void TrayService::RemoveIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayService::UpdateTip(HWND hwnd, bool snappingEnabled)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_TIP;
    wcscpy_s(nid.szTip, snappingEnabled ? kWindowTitle : L"LiteZones (disabled)");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayService::ShowMenu(HWND hwnd, bool snappingEnabled)
{
    POINT pt{};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (!menu)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING | (snappingEnabled ? MF_CHECKED : 0), kMenuToggleSnapping, L"Zone snapping");
    AppendMenuW(menu, MF_STRING, kMenuCycleLayout, L"Cycle layout on monitor");
    AppendMenuW(menu, MF_STRING, kMenuEditLayouts, L"Edit layouts...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuReloadConfig, L"Reload config");
    AppendMenuW(menu, MF_STRING, kMenuOpenFolder, L"Open config folder");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (m_autostart ? MF_CHECKED : 0), kMenuAutostart, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    SetForegroundWindow(hwnd);
    const UINT selected = static_cast<UINT>(TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, nullptr));
    DestroyMenu(menu);

    switch (selected)
    {
    case kMenuToggleSnapping:
        if (m_onToggleSnapping) m_onToggleSnapping();
        break;
    case kMenuCycleLayout:
        if (m_onCycleLayout) m_onCycleLayout();
        break;
    case kMenuEditLayouts:
        if (m_onEditLayouts) m_onEditLayouts();
        break;
    case kMenuReloadConfig:
        if (m_onReloadConfig) m_onReloadConfig();
        break;
    case kMenuOpenFolder:
        if (m_onOpenFolder) m_onOpenFolder();
        break;
    case kMenuAutostart:
        ToggleAutostart();
        break;
    case kMenuExit:
        if (m_onExit) m_onExit();
        break;
    default:
        break;
    }
}

bool TrayService::IsAutostartEnabled() const
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

void TrayService::ToggleAutostart()
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    {
        return;
    }

    m_autostart = !m_autostart;
    if (m_autostart)
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
