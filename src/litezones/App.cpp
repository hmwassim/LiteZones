#include "App.h"

#include "AppZoneHistory.h"
#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "DragController.h"
#include "EditorWindow.h"
#include "FileWatcher.h"
#include "KeyboardSnap.h"
#include "MonitorManager.h"
#include "Paths.h"
#include "Settings.h"
#include "WindowProcessing.h"
#include "WindowProperties.h"
#include "WindowUtils.h"
#include "resource.h"

#include <shellapi.h>

namespace
{
    constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    constexpr UINT kSettingsChangedMessage = WM_APP + 2;
    constexpr UINT kTrayIconId = 1;
    constexpr wchar_t kWindowClassName[] = L"LiteZonesWindow";
    constexpr wchar_t kWindowTitle[] = L"LiteZones";
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValueName[] = L"LiteZones";

    constexpr UINT kMenuToggleSnapping = 40001;
    constexpr UINT kMenuCycleLayout = 40002;
    constexpr UINT kMenuReloadConfig = 40003;
    constexpr UINT kMenuOpenFolder = 40004;
    constexpr UINT kMenuAutostart = 40005;
    constexpr UINT kMenuExit = 40006;
    constexpr UINT kMenuEditLayouts = 40007;

    // The cycle rotates through the built-in templates then every custom layout.
    std::vector<LayoutData> BuildLayoutCycleCandidates()
    {
        std::vector<LayoutData> candidates;
        const std::vector<FancyZonesDataTypes::ZoneSetLayoutType> templates = {
            FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid,
            FancyZonesDataTypes::ZoneSetLayoutType::Grid,
            FancyZonesDataTypes::ZoneSetLayoutType::Rows,
            FancyZonesDataTypes::ZoneSetLayoutType::Columns,
            FancyZonesDataTypes::ZoneSetLayoutType::Focus,
            FancyZonesDataTypes::ZoneSetLayoutType::Blank,
        };
        for (const auto type : templates)
        {
            LayoutData layout;
            layout.type = type;
            layout.showSpacing = DefaultValues::ShowSpacing;
            layout.spacing = DefaultValues::Spacing;
            layout.zoneCount = DefaultValues::ZoneCount;
            layout.sensitivityRadius = DefaultValues::SensitivityRadius;
            candidates.push_back(layout);
        }
        for (const auto& [uuid, data] : CustomLayouts::instance().AllLayouts())
        {
            (void)data;
            if (const auto layout = CustomLayouts::instance().GetLayout(uuid); layout.has_value())
            {
                candidates.push_back(*layout);
            }
        }
        return candidates;
    }
}

App::App(HINSTANCE hInstance) :
    m_hInstance(hInstance),
    m_workAreaManager(hInstance)
{
}

App::~App()
{
    if (m_hooks)
    {
        m_hooks->Stop();
    }
    m_dragController.reset();
    if (m_fileWatcher)
    {
        m_fileWatcher->Stop();
    }
    if (m_editor)
    {
        m_editor->Close();
    }
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

bool App::Init()
{
    if (!Paths::EnsureConfigDir())
    {
        return false;
    }

    Settings::instance().Load();
    AppZoneHistory::instance().LoadData();
    CustomLayouts::instance().LoadData();
    AppliedLayouts::instance().LoadData();
    m_autostart = IsAutostartEnabled();
    ReloadWorkAreas();

    if (!CreateHiddenWindow())
    {
        return false;
    }
    if (!AddTrayIcon())
    {
        return false;
    }

    m_dragController = std::make_unique<DragController>(m_workAreaManager);
    m_keyboardSnap = std::make_unique<KeyboardSnap>(m_workAreaManager);
    m_hooks = std::make_unique<Hooks>(m_hwnd);
    if (!m_hooks->Start())
    {
        return false;
    }

    m_fileWatcher = std::make_unique<FileWatcher>();
    m_fileWatcher->Start(m_hwnd, kSettingsChangedMessage, Paths::ConfigDir(), { L"settings.json", L"custom-layouts.json", L"applied-layouts.json" });

    // Debug hook: set LITEZONES_OPEN_EDITOR=1 to open the layout editor on start.
    wchar_t openEditor[2]{};
    if (GetEnvironmentVariableW(L"LITEZONES_OPEN_EDITOR", openEditor, 2) > 0 && openEditor[0] == L'1')
    {
        OpenLayoutEditor();
    }
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
    AppendMenuW(menu, MF_STRING, kMenuCycleLayout, L"Cycle layout on monitor");
    AppendMenuW(menu, MF_STRING, kMenuEditLayouts, L"Edit layouts...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuReloadConfig, L"Reload config");
    AppendMenuW(menu, MF_STRING, kMenuOpenFolder, L"Open config folder");
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
    case kMenuCycleLayout:
        CycleLayoutOnMonitor();
        break;
    case kMenuEditLayouts:
        OpenLayoutEditor();
        break;
    case kMenuReloadConfig:
        ReloadConfig();
        break;
    case kMenuOpenFolder:
        OpenConfigFolder();
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
    if (!m_snappingEnabled && m_dragController)
    {
        m_dragController->MoveSizeEnd();
    }
    if (m_hooks)
    {
        m_hooks->SetSnappingEnabled(m_snappingEnabled);
    }
    UpdateTrayTip();
}

void App::ReloadConfig()
{
    Settings::instance().Load();
    CustomLayouts::instance().LoadData();
    AppliedLayouts::instance().LoadData();
    ReloadWorkAreas();
    UpdateTrayTip();
}

void App::ReloadWorkAreas(bool forceRelayout)
{
    // Abort any active drag first: it holds pointers into m_workAreaManager.
    if (m_dragController)
    {
        m_dragController->MoveSizeEnd();
        if (m_hooks)
        {
            m_hooks->DisableLocationChangeTracking();
        }
    }

    LayoutData layout;
    layout.type = FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid;
    layout.zoneCount = DefaultValues::ZoneCount;
    m_workAreaManager.Update(Settings::instance().data.spanZonesAcrossMonitors, layout, forceRelayout);
}

void App::CycleLayoutOnMonitor()
{
    POINT pt{};
    GetCursorPos(&pt);
    const HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);

    LayoutData defaultLayout;
    defaultLayout.type = FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid;
    defaultLayout.zoneCount = DefaultValues::ZoneCount;
    const LayoutData current = LayoutResolver::Resolve(monitor, Settings::instance().data.spanZonesAcrossMonitors, defaultLayout);

    const std::vector<LayoutData> candidates = BuildLayoutCycleCandidates();
    size_t index = candidates.size();
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        if (candidates[i].type == current.type && candidates[i].uuid == current.uuid && candidates[i].zoneCount == current.zoneCount)
        {
            index = i;
            break;
        }
    }
    const size_t next = (index + 1) % candidates.size();

    const std::wstring deviceKey = MonitorUtils::GetDeviceKey(monitor);
    AppliedLayouts::instance().ApplyLayout(deviceKey, candidates[next]);
    AppliedLayouts::instance().SaveData();
    ReloadWorkAreas();
}

void App::OpenLayoutEditor()
{
    if (!m_editor)
    {
        m_editor = std::make_unique<EditorWindow>(m_hInstance, m_hwnd);
        m_editor->SetOnChanged([this] { ReloadConfig(); });
    }
    if (!m_editor->IsOpen() && !m_editor->Create())
    {
        m_editor.reset();
        return;
    }
    m_editor->Show();
}

void App::OpenConfigFolder()
{
    const std::wstring dir = Paths::ConfigDir();
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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

void App::HandleMoveSizeStart(HWND window)
{
    if (!m_dragController)
    {
        return;
    }

    m_dragController->MoveSizeStart(window);
    if (m_dragController->IsDragging())
    {
        if (m_hooks)
        {
            m_hooks->EnableLocationChangeTracking();
        }
        m_dragController->MoveSizeUpdate();
    }
}

void App::HandleMoveSizeUpdate()
{
    if (m_dragController)
    {
        m_dragController->MoveSizeUpdate();
    }
}

void App::HandleMoveSizeEnd()
{
    if (m_dragController)
    {
        m_dragController->MoveSizeEnd();
    }
    if (m_hooks)
    {
        m_hooks->DisableLocationChangeTracking();
    }
}

void App::HandleWindowDestroyed(HWND window)
{
    if (m_dragController)
    {
        m_dragController->OnWindowDestroyed(window);
    }
    if (m_hooks)
    {
        m_hooks->DisableLocationChangeTracking();
    }
}

void App::HandleSnapHotkey(DWORD vkCode)
{
    if (!m_snappingEnabled || !m_keyboardSnap)
    {
        return;
    }

    // Normalize numpad digits to their '0'-'9' equivalents.
    if (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9)
    {
        vkCode = static_cast<DWORD>('0') + (vkCode - VK_NUMPAD0);
    }

    m_keyboardSnap->HandleKey(GetForegroundWindow(), vkCode);
}

void App::HandleWindowCreated(HWND window)
{
    if (!m_snappingEnabled)
    {
        return;
    }

    if (!WindowProcessing::IsProcessableManually(window))
    {
        return;
    }

    // Already snapped (e.g. re-shown or moved): leave it alone.
    if (!RetrieveZoneIndexProperty(window).ToIndexSet().empty())
    {
        return;
    }

    const std::wstring processPath = WindowUtils::GetProcessPath(window);
    if (processPath.empty())
    {
        return;
    }
    const ZoneIndexSet zones = AppZoneHistory::instance().GetAppLastZoneIndexSet(processPath);
    if (zones.empty())
    {
        return;
    }

    POINT pt{};
    GetCursorPos(&pt);
    WorkArea* workArea = m_workAreaManager.WorkAreaContainingPoint(pt);
    if (!workArea)
    {
        const HMONITOR primary = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        workArea = m_workAreaManager.WorkAreaFor(primary);
    }
    if (!workArea)
    {
        return;
    }

    workArea->Snap(window, zones);
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

    case kSettingsChangedMessage:
        ReloadConfig();
        return 0;

    case WM_DISPLAYCHANGE:
        if (m_dragController)
        {
            m_dragController->MoveSizeEnd();
        }
        ReloadWorkAreas(/*forceRelayout=*/false);
        return 0;

    case WM_PRIV_MOVESIZESTART:
        if (m_snappingEnabled)
        {
            HandleMoveSizeStart(reinterpret_cast<HWND>(wParam));
        }
        return 0;

    case WM_PRIV_MOVESIZEEND:
        HandleMoveSizeEnd();
        return 0;

    case WM_PRIV_LOCATIONCHANGE:
        HandleMoveSizeUpdate();
        return 0;

    case WM_PRIV_WINDOWDESTROYED:
        HandleWindowDestroyed(reinterpret_cast<HWND>(wParam));
        return 0;

    case WM_PRIV_KEYSTATE:
        if (m_dragController)
        {
            m_dragController->OnKeyStateChanged(static_cast<UINT>(wParam), lParam != 0);
        }
        return 0;

    case WM_PRIV_MOUSEBUTTON:
        if (m_dragController)
        {
            m_dragController->OnMouseButtonChanged(static_cast<UINT>(wParam), lParam != 0);
        }
        return 0;

    case WM_PRIV_SNAP_HOTKEY:
        HandleSnapHotkey(static_cast<DWORD>(wParam));
        return 0;

    case WM_PRIV_WINDOWCREATED:
        HandleWindowCreated(reinterpret_cast<HWND>(wParam));
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
