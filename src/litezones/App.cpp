#include "App.h"

#include "AppZoneHistory.h"
#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "DragController.h"
#include "EditorWindow.h"
#include "FileWatcher.h"
#include "KeyboardSnap.h"
#include "LayoutHelpers.h"
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
    constexpr UINT kSettingsChangedMessage = WM_APP + 2;
    constexpr UINT kFlushTimerId = 1;
    constexpr wchar_t kWindowClassName[] = L"LiteZonesWindow";
    constexpr wchar_t kWindowTitle[] = L"LiteZones";

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
    m_workAreaManager(hInstance, Settings::instance().data)
{
}

App::~App()
{
    if (m_hwnd)
    {
        Exit();
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
    ReloadWorkAreas();

    if (!CreateHiddenWindow())
    {
        return false;
    }

    m_tray.SetOnToggleSnapping([this] { ToggleSnapping(); });
    m_tray.SetOnCycleLayout([this] { CycleLayoutOnMonitor(); });
    m_tray.SetOnEditLayouts([this] { OpenLayoutEditor(); });
    m_tray.SetOnReloadConfig([this] { ReloadConfig(); });
    m_tray.SetOnOpenFolder([this] { OpenConfigFolder(); });
    m_tray.SetOnExit([this] { Exit(); });

    if (!m_tray.AddIcon(m_hwnd, m_hInstance))
    {
        return false;
    }

    m_dragController = std::make_unique<DragController>(m_workAreaManager, Settings::instance().data);
    m_keyboardSnap = std::make_unique<KeyboardSnap>(m_workAreaManager, Settings::instance().data);
    m_hooks = std::make_unique<Hooks>(m_hwnd, Settings::instance().data);
    if (!m_hooks->Start())
    {
        return false;
    }

    m_fileWatcher = std::make_unique<FileWatcher>();
    m_fileWatcher->Start(m_hwnd, kSettingsChangedMessage, Paths::ConfigDir(), { L"settings.json", L"custom-layouts.json", L"applied-layouts.json" });

    SetTimer(m_hwnd, kFlushTimerId, 500, nullptr);

    return true;
}

int App::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (m_editor && m_editor->IsOpen() && m_editor->GetAccel())
        {
            if (TranslateAcceleratorW(m_editor->Hwnd(), m_editor->GetAccel(), &msg))
            {
                continue;
            }
        }
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
    m_tray.UpdateTip(m_hwnd, m_snappingEnabled);
}

void App::ReloadConfig()
{
    Settings::instance().Load();
    CustomLayouts::instance().LoadData();
    AppliedLayouts::instance().LoadData();
    ReloadWorkAreas();
    m_tray.UpdateTip(m_hwnd, m_snappingEnabled);
}

void App::ReloadWorkAreas(bool forceRelayout)
{
    if (m_dragController)
    {
        m_dragController->MoveSizeEnd();
        if (m_hooks)
        {
            m_hooks->DisableLocationChangeTracking();
        }
    }

    LayoutData layout = LayoutHelpers::MakeDefaultLayout();
    m_workAreaManager.Update(Settings::instance().data.spanZonesAcrossMonitors, layout, forceRelayout);
}

void App::CycleLayoutOnMonitor()
{
    POINT pt{};
    GetCursorPos(&pt);
    const HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);

    const LayoutData defaultLayout = LayoutHelpers::MakeDefaultLayout();
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
        m_editor = std::make_unique<EditorWindow>(m_hInstance, m_hwnd, Settings::instance().data);
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

void App::Exit()
{
    KillTimer(m_hwnd, kFlushTimerId);
    AppZoneHistory::instance().FlushIfDirty();
    if (m_hooks)
    {
        m_hooks->Stop();
    }
    if (m_fileWatcher)
    {
        m_fileWatcher->Stop();
    }
    if (m_editor)
    {
        m_editor->Close();
    }
    m_tray.RemoveIcon();
    m_hwnd = nullptr;
    ExitProcess(0);
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

    if (!WindowProcessing::IsProcessableManually(window, Settings::instance().data))
    {
        return;
    }

    if (!RetrieveZoneIndexProperty(window).ToIndexSet().empty())
    {
        return;
    }

    // Windows already remembers window placement via SetWindowPlacement (called
    // by SizeWindowToRect during snap). Let it restore the position natively
    // instead of overriding with process-path-based history, which breaks
    // multi-window apps (e.g. Chrome profiles sharing the same chrome.exe).
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
    constexpr UINT kTrayCallbackMessage = WM_APP + 1;

    switch (msg)
    {
    case kTrayCallbackMessage:
        if (LOWORD(lParam) == WM_LBUTTONUP)
        {
            OpenLayoutEditor();
        }
        else if (LOWORD(lParam) == WM_RBUTTONUP)
        {
            m_tray.ShowMenu(m_hwnd, m_snappingEnabled);
        }
        return 0;

    case kSettingsChangedMessage:
        ReloadConfig();
        return 0;

    case WM_TIMER:
        if (wParam == kFlushTimerId)
        {
            AppZoneHistory::instance().FlushIfDirty();
        }
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
        KillTimer(m_hwnd, kFlushTimerId);
        AppZoneHistory::instance().FlushIfDirty();
        if (m_hooks)
        {
            m_hooks->Stop();
        }
        m_tray.RemoveIcon();
        m_hwnd = nullptr;
        ExitProcess(0);
        return 0;

    case WM_CLOSE:
        Exit();
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
