#include "EditorWindowInternal.h"

#include "AppliedLayouts.h"
#include "EditorCanvas.h"
#include "LayoutEngine.h"
#include "Settings.h"
#include "SettingsDialog.h"

#include <objbase.h>

EditorWindow::EditorWindow(HINSTANCE hInstance, HWND notifyWindow, const SettingsData& settings) :
    m_hInstance(hInstance),
    m_notifyWindow(notifyWindow),
    m_settings(settings)
{
}

EditorWindow::~EditorWindow()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool EditorWindow::Create()
{
    using namespace EditorWindowInternal;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &EditorWindow::WndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kEditorClassName;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    const std::vector<MonitorUtils::MonitorRect> monitors = MonitorUtils::GetAllMonitorWorkRects();
    UINT dpi = 96;
    if (!monitors.empty())
    {
        dpi = MonitorUtils::GetDpiForMonitor(monitors.front().first);
    }
    m_currentDpi = dpi;

    const int initialWidth = LayoutHelpers::ScaleForDpi(kEditorInitialWidth, dpi);
    const int initialHeight = LayoutHelpers::ScaleForDpi(kEditorInitialHeight, dpi);

    m_hwnd = CreateWindowExW(WS_CLIPCHILDREN, kEditorClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, initialWidth, initialHeight, nullptr, nullptr, m_hInstance, this);
    if (!m_hwnd)
    {
        return false;
    }

    if (!CreateControls())
    {
        return false;
    }
    CreateMenuBar();
    PopulateMonitorCombo();
    PopulateLayoutList();
    SelectActiveLayout();

    m_hAccel = LoadAcceleratorsW(m_hInstance, MAKEINTRESOURCEW(IDA_EDITOR));
    return true;
}

bool EditorWindow::IsEditFocused() const
{
    if (!m_hwnd)
    {
        return false;
    }
    HWND focused = GetFocus();
    if (!focused)
    {
        return false;
    }
    wchar_t className[32]{};
    GetClassNameW(focused, className, static_cast<int>(std::size(className)));
    return wcscmp(className, L"Edit") == 0;
}

void EditorWindow::Show()
{
    if (!m_hwnd)
    {
        return;
    }
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
}

void EditorWindow::Close()
{
    if (m_hwnd)
    {
        PersistAllWorkingCopies();
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LiteZonesTypes::CustomLayoutData* EditorWindow::EnsureWorkingCopy(const GUID& uuid)
{
    const auto it = m_workingCopies.find(uuid);
    if (it != m_workingCopies.end())
    {
        return &it->second;
    }
    const auto* storeData = CustomLayouts::instance().GetCustomLayoutData(uuid);
    if (!storeData)
    {
        return nullptr;
    }
    return &m_workingCopies.emplace(uuid, *storeData).first->second;
}

void EditorWindow::PersistAllWorkingCopies()
{
    for (const auto& [uuid, data] : m_workingCopies)
    {
        CustomLayouts::instance().AddLayout(uuid, data);
    }
}

void EditorWindow::DiscardWorkingCopy(const GUID& uuid)
{
    m_workingCopies.erase(uuid);
}

bool EditorWindow::WorkingCopyDiffersFromStore() const
{
    for (const auto& [uuid, wcData] : m_workingCopies)
    {
        const auto* storeData = CustomLayouts::instance().GetCustomLayoutData(uuid);
        if (!storeData)
        {
            return true;
        }
        if (!(wcData == *storeData))
        {
            return true;
        }
    }
    return false;
}

bool EditorWindow::HasUnsavedChanges() const
{
    return WorkingCopyDiffersFromStore();
}

void EditorWindow::NotifyChanged()
{
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void EditorWindow::UpdateHint()
{
    if (!m_staticHint)
    {
        return;
    }

    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        SetWindowTextW(m_staticHint, L"");
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    if (entry.isTemplate)
    {
        SetWindowTextW(m_staticHint,
                       L"Templates: set the number of zones and spacing above. To resize zones with the mouse, "
                       L"create a custom layout with New...");
    }
    else
    {
        if (const auto* data = EnsureWorkingCopy(entry.uuid))
        {
            if (data->type == LiteZonesTypes::CustomLayoutType::Grid)
            {
                SetWindowTextW(m_staticHint,
                               L"Grid: drag separators to resize; double-click to split; "
                               L"Ctrl+click to multi-select, right-click to merge. "
                               L"Arrows: nudge, Del: remove zone, Ctrl+Z/Y: undo/redo, Esc: cancel.");
            }
            else
            {
                SetWindowTextW(m_staticHint,
                               L"Canvas: drag empty space to draw a zone, drag a zone to move it, drag a handle to "
                               L"resize it; right-click a zone to delete it. "
                               L"Arrows: nudge, Ctrl+Z/Y: undo/redo, Esc: cancel.");
            }
        }
    }
}

void EditorWindow::UpdateUndoState()
{
    if (m_menuEdit)
    {
        EnableMenuItem(m_menuEdit, IDM_EDIT_UNDO,
                       MF_BYCOMMAND | (m_undoStack.empty() ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(m_menuEdit, IDM_EDIT_REDO,
                       MF_BYCOMMAND | (m_redoStack.empty() ? MF_GRAYED : MF_ENABLED));
    }
}

void EditorWindow::SelectActiveLayout()
{
    const std::vector<MonitorUtils::MonitorRect> monitors = MonitorUtils::GetWorkAreas(false);
    if (monitors.empty())
    {
        return;
    }

    const std::wstring primaryDeviceKey = MonitorUtils::GetDeviceKey(monitors.front().first);

    for (size_t i = 0; i < m_deviceKeys.size(); ++i)
    {
        if (m_deviceKeys[i] == primaryDeviceKey)
        {
            SendMessageW(m_monitorCombo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);

            const auto layoutData = AppliedLayouts::instance().GetDeviceLayout(primaryDeviceKey);
            if (layoutData.has_value())
            {
                const LayoutData& applied = *layoutData;
                for (size_t j = 0; j < m_entries.size(); ++j)
                {
                    bool match = false;
                    if (applied.type == LiteZonesTypes::ZoneSetLayoutType::Custom)
                    {
                        match = !m_entries[j].isTemplate && IsEqualGUID(m_entries[j].uuid, applied.uuid);
                    }
                    else
                    {
                        match = m_entries[j].isTemplate && m_entries[j].type == applied.type;
                    }
                    if (match)
                    {
                        SendMessageW(m_listBox, LB_SETCURSEL, static_cast<WPARAM>(j), 0);
                        OnSelectionChanged();
                        return;
                    }
                }
            }
            return;
        }
    }
}

LRESULT CALLBACK EditorWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EditorWindow* self = reinterpret_cast<EditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<EditorWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self)
    {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT EditorWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using namespace EditorWindowInternal;

    switch (msg)
    {
    case WM_SIZE:
        LayoutControls();
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = LayoutHelpers::ScaleForDpi(kMinTrackWidth, m_currentDpi);
        info->ptMinTrackSize.y = LayoutHelpers::ScaleForDpi(kMinTrackHeight, m_currentDpi);
        return 0;
    }

    case WM_DPICHANGED:
    {
        m_currentDpi = HIWORD(wParam);
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutControls();
        return 0;
    }

    case WM_DISPLAYCHANGE:
        PopulateMonitorCombo();
        UpdateCanvasPreview();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kListLayouts:
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                OnSelectionChanged();
            }
            return 0;
        case kMonitorCombo:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                UpdateCanvasPreview();
                UpdateApplyButtons();
            }
            return 0;
        case kBtnNew:
            OnNewLayout();
            return 0;
        case kBtnDuplicate:
            OnDuplicate();
            return 0;
        case kBtnDelete:
            OnDelete();
            return 0;
        case kBtnRename:
            OnRename();
            return 0;
        case kBtnApply:
            OnApply();
            return 0;
        case kBtnApplyAll:
            OnApplyAll();
            return 0;
        case kEditSpacing:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                OnSpacingChanged();
            }
            return 0;
        case kEditZoneCount:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                OnZoneCountChanged();
            }
            return 0;
        case IDM_FILE_SAVE:
            OnSave();
            return 0;
        case IDM_FILE_NEW:
            OnNewLayout();
            return 0;
        case IDM_FILE_APPLY:
            OnApply();
            return 0;
        case IDM_FILE_APPLYALL:
            OnApplyAll();
            return 0;
        case IDM_FILE_CLOSE:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case IDM_EDIT_UNDO:
            OnUndo();
            return 0;
        case IDM_EDIT_REDO:
            OnRedo();
            return 0;
        case IDM_EDIT_RENAME:
            OnRename();
            return 0;
        case IDM_EDIT_DELETE:
            OnDelete();
            return 0;
        case IDM_EDIT_DUPLICATE:
            OnDuplicate();
            return 0;
        case IDM_EDIT_SETTINGS:
            if (SettingsDialog::Show(m_hwnd, m_hInstance))
            {
                NotifyChanged();
            }
            return 0;
        case IDM_HELP_ABOUT:
            OnAbout();
            return 0;
        }
        break;

    case WM_CLOSE:
        if (HasUnsavedChanges())
        {
            const int result = MessageBoxW(hwnd,
                                           L"Save changes to layouts before closing?",
                                           L"LiteZones Editor",
                                           MB_YESNOCANCEL | MB_ICONQUESTION);
            if (result == IDCANCEL)
            {
                return 0;
            }
            if (result == IDYES)
            {
                PersistAllWorkingCopies();
                NotifyChanged();
            }
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
        m_hwnd = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
