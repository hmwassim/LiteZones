#include "TrayService.h"
#include "resource.h"
#include <shellapi.h>

namespace
{
    constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    constexpr UINT kTrayIconId = 1;

    constexpr UINT kMenuToggleSnapping = 40001;
    constexpr UINT kMenuCycleLayout = 40002;
    constexpr UINT kMenuReloadConfig = 40003;
    constexpr UINT kMenuOpenFolder = 40004;
    constexpr UINT kMenuExit = 40006;
    constexpr UINT kMenuEditLayouts = 40007;
    constexpr UINT kMenuSettings = 40008;
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
    wcscpy_s(nid.szTip, L"LiteZones - Click to open editor");
    if (Shell_NotifyIconW(NIM_ADD, &nid) == FALSE)
    {
        return false;
    }

    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfoTitle, L"LiteZones");
    wcscpy_s(nid.szInfo, L"LiteZones is running. Shift+drag windows to snap.\nClick tray icon to open editor.");
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    return true;
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
    wcscpy_s(nid.szTip, snappingEnabled ? L"LiteZones - Click to open editor" : L"LiteZones (disabled) - Click to open editor");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayService::ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* message)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, message);
    nid.dwInfoFlags = NIIF_INFO;
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
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuReloadConfig, L"Reload config");
    AppendMenuW(menu, MF_STRING, kMenuOpenFolder, L"Open config folder");
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
    case kMenuSettings:
        if (m_onSettings) m_onSettings();
        break;
    case kMenuReloadConfig:
        if (m_onReloadConfig) m_onReloadConfig();
        break;
    case kMenuOpenFolder:
        if (m_onOpenFolder) m_onOpenFolder();
        break;
    case kMenuExit:
        if (m_onExit) m_onExit();
        break;
    default:
        break;
    }
}
