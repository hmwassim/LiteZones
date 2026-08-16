#include "App.h"

#include "resource.h"

#include <shellapi.h>

namespace
{
    constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    constexpr UINT kTrayIconId = 1;
    constexpr wchar_t kWindowClassName[] = L"LiteZonesWindow";
    constexpr wchar_t kWindowTitle[] = L"LiteZones";
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValueName[] = L"LiteZones";

    constexpr UINT kMenuToggleSnapping = 40001;
    constexpr UINT kMenuAutostart = 40002;
    constexpr UINT kMenuExit = 40003;
}

App::App(HINSTANCE hInstance) :
    m_hInstance(hInstance)
{
}

App::~App()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

bool App::Init()
{
    if (!CreateHiddenWindow())
    {
        return false;
    }
    if (!AddTrayIcon())
    {
        return false;
    }
    m_autostart = IsAutostartEnabled();
    return true;
}

int App::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

bool App::CreateHiddenWindow()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::WndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&wc) == 0)
    {
        return false;
    }

    m_hwnd = CreateWindowExW(0, kWindowClassName, kWindowTitle, 0, 0, 0, 0, 0, nullptr, nullptr, m_hInstance, this);
    return m_hwnd != nullptr;
}

bool App::AddTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMessage;
    nid.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_APP));
    wcscpy_s(nid.szTip, kWindowTitle);
    return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void App::RemoveTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void App::ShowTrayMenu()
{
    POINT pt{};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (!menu)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING | (m_snappingEnabled ? MF_CHECKED : 0), kMenuToggleSnapping, L"Zone snapping");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (m_autostart ? MF_CHECKED : 0), kMenuAutostart, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    SetForegroundWindow(m_hwnd);
    const UINT selected = static_cast<UINT>(TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, nullptr));
    DestroyMenu(menu);

    switch (selected)
    {
    case kMenuToggleSnapping:
        ToggleSnapping();
        break;
    case kMenuAutostart:
        ToggleAutostart();
        break;
    case kMenuExit:
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

void App::ToggleSnapping()
{
    m_snappingEnabled = !m_snappingEnabled;
    UpdateTrayTip();
}

void App::ToggleAutostart()
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

bool App::IsAutostartEnabled() const
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

void App::UpdateTrayTip()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_TIP;
    wcscpy_s(nid.szTip, m_snappingEnabled ? kWindowTitle : L"LiteZones (disabled)");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self)
    {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case kTrayCallbackMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP)
        {
            ShowTrayMenu();
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
