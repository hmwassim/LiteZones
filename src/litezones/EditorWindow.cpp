#include "EditorWindow.h"

#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "EditorCanvas.h"
#include "GridData.h"
#include "LayoutEngine.h"
#include "LayoutHelpers.h"
#include "MonitorManager.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "resource.h"

#include <objbase.h>

#include <algorithm>

namespace
{
    constexpr wchar_t kEditorClassName[] = L"LiteZonesEditorWindow";
    constexpr wchar_t kWindowTitle[] = L"LiteZones - Layout Editor";

    constexpr int kLeftPanelWidth = 216;
    constexpr int kDefaultVirtualWidth = 1600;
    constexpr int kDefaultVirtualHeight = 900;
    constexpr int kEditorInitialWidth = 900;
    constexpr int kEditorInitialHeight = 620;
    constexpr int kMaxZoneCount = 16;
    constexpr int kMaxSpacing = 100;
    constexpr int kMinTrackWidth = 820;
    constexpr int kMinTrackHeight = 480;

    int ScaleForDpi(int value, UINT dpi)
    {
        return MulDiv(value, static_cast<int>(dpi), 96);
    }

    struct NewLayoutResult
    {
        std::wstring name;
        bool grid = true;
        int rows = 1;
        int columns = 1;
    };

    std::wstring Trim(const std::wstring& text)
    {
        const size_t first = text.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos)
        {
            return {};
        }
        const size_t last = text.find_last_not_of(L" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    std::wstring MakeUniqueName(const std::wstring& base)
    {
        const auto& layouts = CustomLayouts::instance().AllLayouts();
        std::wstring candidate = base;
        int suffix = 2;
        while (std::any_of(layouts.begin(), layouts.end(),
                           [&candidate](const auto& entry) { return entry.second.name == candidate; }))
        {
            wchar_t buffer[32]{};
            swprintf_s(buffer, L" (%d)", suffix++);
            candidate = base + buffer;
        }
        return candidate;
    }

    void GetPrimaryWorkArea(int& width, int& height)
    {
        width = kDefaultVirtualWidth;
        height = kDefaultVirtualHeight;
        const std::vector<MonitorUtils::MonitorRect> monitors = MonitorUtils::GetAllMonitorWorkRects();
        if (!monitors.empty())
        {
            const RECT& rect = monitors.front().second;
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }
    }

    INT_PTR CALLBACK NewLayoutDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
        {
            SetWindowLongPtrW(dlg, GWLP_USERDATA, lParam);
            CheckDlgButton(dlg, IDC_NEW_GRID, BST_CHECKED);
            SetDlgItemInt(dlg, IDC_NEW_ROWS, 2, FALSE);
            SetDlgItemInt(dlg, IDC_NEW_COLS, 2, FALSE);
            HWND nameEdit = GetDlgItem(dlg, IDC_NEW_NAME);
            SetWindowTextW(nameEdit, L"Custom Layout");
            SendMessageW(nameEdit, EM_SETSEL, 0, -1);
            SetFocus(nameEdit);
            return FALSE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_NEW_GRID:
            case IDC_NEW_CANVAS:
            {
                const BOOL isGrid = IsDlgButtonChecked(dlg, IDC_NEW_GRID) == BST_CHECKED;
                EnableWindow(GetDlgItem(dlg, IDC_NEW_ROWS), isGrid);
                EnableWindow(GetDlgItem(dlg, IDC_NEW_COLS), isGrid);
                return TRUE;
            }
            case IDOK:
            {
                auto* result = reinterpret_cast<NewLayoutResult*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
                wchar_t buffer[128]{};
                GetDlgItemTextW(dlg, IDC_NEW_NAME, buffer, static_cast<int>(std::size(buffer)));
                result->name = Trim(buffer);
                result->grid = IsDlgButtonChecked(dlg, IDC_NEW_GRID) == BST_CHECKED;
                if (result->grid)
                {
                    result->rows = std::max(1, std::min(kMaxZoneCount, static_cast<int>(GetDlgItemInt(dlg, IDC_NEW_ROWS, nullptr, FALSE))));
                    result->columns = std::max(1, std::min(kMaxZoneCount, static_cast<int>(GetDlgItemInt(dlg, IDC_NEW_COLS, nullptr, FALSE))));
                }
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
        return FALSE;
    }

    INT_PTR CALLBACK RenameLayoutDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
            SetWindowLongPtrW(dlg, GWLP_USERDATA, lParam);
            SetFocus(GetDlgItem(dlg, IDC_RENAME_NAME));
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDOK:
            {
                auto* result = reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
                wchar_t buffer[128]{};
                GetDlgItemTextW(dlg, IDC_RENAME_NAME, buffer, static_cast<int>(std::size(buffer)));
                *result = Trim(buffer);
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
        return FALSE;
    }
}

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

    const int initialWidth = ScaleForDpi(kEditorInitialWidth, dpi);
    const int initialHeight = ScaleForDpi(kEditorInitialHeight, dpi);

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

bool EditorWindow::CreateControls()
{
    const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    m_listBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kListLayouts), m_hInstance, nullptr);
    m_staticMonitor = CreateWindowExW(0, L"STATIC", L"Monitor:", WS_CHILD | WS_VISIBLE,
                                      0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
    m_monitorCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         CBS_DROPDOWNLIST | WS_VSCROLL,
                                     0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kMonitorCombo), m_hInstance, nullptr);
    m_staticSpacing = CreateWindowExW(0, L"STATIC", L"Spacing:", WS_CHILD | WS_VISIBLE,
                                      0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
    m_spacingEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                    0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kEditSpacing), m_hInstance, nullptr);
    m_staticZones = CreateWindowExW(0, L"STATIC", L"Zones:", WS_CHILD | WS_VISIBLE,
                                    0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
    m_zoneCountEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kEditZoneCount), m_hInstance, nullptr);
    m_staticHint = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
    m_canvas = EditorCanvas::Create(m_hwnd, m_hInstance);
    EditorCanvas::SetSettings(m_canvas, m_settings);
    if (!m_listBox || !m_monitorCombo || !m_canvas)
    {
        return false;
    }

    EditorCanvas::SetOnEdited(m_canvas, [this]() { m_dirty = true; NotifyChanged(); });
    EditorCanvas::SetOnBeforeEdit(m_canvas, [this]() { PushUndoSnapshot(); });
    EditorCanvas::SetOnHint(m_canvas, [this](const wchar_t* msg) { SetWindowTextW(m_staticHint, msg); });

    const auto createButton = [this, font](ControlId id, const wchar_t* text) {
        return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                               0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(id), m_hInstance, nullptr);
    };
    HWND buttons[] = {
        createButton(kBtnNew, L"New..."),
        createButton(kBtnDuplicate, L"Duplicate"),
        createButton(kBtnDelete, L"Delete"),
        createButton(kBtnRename, L"Rename..."),
    };

    m_btnApply = CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                  0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kBtnApply), m_hInstance, nullptr);
    m_btnApplyAll = CreateWindowExW(0, L"BUTTON", L"Apply All", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                    0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(kBtnApplyAll), m_hInstance, nullptr);

    for (HWND control : { m_listBox, m_staticMonitor, m_monitorCombo, m_staticSpacing, m_spacingEdit,
                          m_staticZones, m_zoneCountEdit, m_staticHint, m_canvas, m_btnApply, m_btnApplyAll })
    {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    for (HWND control : buttons)
    {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    return true;
}

void EditorWindow::LayoutControls()
{
    if (!m_hwnd)
    {
        return;
    }
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const UINT dpi = m_currentDpi;

    const int panelW = ScaleForDpi(kLeftPanelWidth, dpi);
    const int pad = ScaleForDpi(8, dpi);
    const int ctrlH = ScaleForDpi(24, dpi);
    const int labelH = ScaleForDpi(16, dpi);
    const int listHeight = std::max(ScaleForDpi(120, dpi), height - pad - ScaleForDpi(64, dpi));
    MoveWindow(m_listBox, pad, pad, panelW - pad * 2, listHeight, TRUE);
    MoveWindow(m_staticMonitor, panelW + pad, ScaleForDpi(13, dpi), ScaleForDpi(56, dpi), labelH, TRUE);
    MoveWindow(m_monitorCombo, panelW + ScaleForDpi(66, dpi), ScaleForDpi(10, dpi), ScaleForDpi(170, dpi), ScaleForDpi(200, dpi), TRUE);
    MoveWindow(m_staticSpacing, panelW + ScaleForDpi(244, dpi), ScaleForDpi(13, dpi), ScaleForDpi(48, dpi), labelH, TRUE);
    MoveWindow(m_spacingEdit, panelW + ScaleForDpi(296, dpi), ScaleForDpi(10, dpi), ScaleForDpi(44, dpi), ctrlH, TRUE);
    MoveWindow(m_staticZones, panelW + ScaleForDpi(348, dpi), ScaleForDpi(13, dpi), ScaleForDpi(40, dpi), labelH, TRUE);
    MoveWindow(m_zoneCountEdit, panelW + ScaleForDpi(390, dpi), ScaleForDpi(10, dpi), ScaleForDpi(40, dpi), ctrlH, TRUE);
    MoveWindow(m_btnApply, panelW + ScaleForDpi(438, dpi), ScaleForDpi(10, dpi), ScaleForDpi(56, dpi), ctrlH, TRUE);
    MoveWindow(m_btnApplyAll, panelW + ScaleForDpi(500, dpi), ScaleForDpi(10, dpi), ScaleForDpi(72, dpi), ctrlH, TRUE);

    const int buttonWidth = (panelW - pad * 2 - pad) / 2;
    MoveWindow(GetDlgItem(m_hwnd, kBtnNew), pad, height - ScaleForDpi(60, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDuplicate), pad + buttonWidth + pad, height - ScaleForDpi(60, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDelete), pad, height - ScaleForDpi(32, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnRename), pad + buttonWidth + pad, height - ScaleForDpi(32, dpi), buttonWidth, ctrlH, TRUE);

    MoveWindow(m_canvas, panelW + pad, ScaleForDpi(40, dpi), std::max(ScaleForDpi(60, dpi), width - panelW - pad * 2), std::max(ScaleForDpi(60, dpi), height - ScaleForDpi(40, dpi) - ScaleForDpi(32, dpi)), TRUE);
    MoveWindow(m_staticHint, panelW + pad, height - ScaleForDpi(24, dpi), std::max(ScaleForDpi(60, dpi), width - panelW - pad * 2), ScaleForDpi(18, dpi), TRUE);
}

void EditorWindow::PopulateLayoutList()
{
    const int previousIndex = SelectedListIndex();
    GUID previousUuid = GUID_NULL;
    if (previousIndex >= 0 && previousIndex < static_cast<int>(m_entries.size()))
    {
        previousUuid = m_entries[static_cast<size_t>(previousIndex)].uuid;
    }

    m_entries.clear();
    const auto addTemplate = [this](LiteZonesTypes::ZoneSetLayoutType type, const wchar_t* name) {
        ListEntry entry;
        entry.isTemplate = true;
        entry.type = type;
        entry.uuid = GUID_NULL;
        entry.name = name;
        m_entries.push_back(entry);
    };
    addTemplate(LiteZonesTypes::ZoneSetLayoutType::Rows, L"Rows");
    addTemplate(LiteZonesTypes::ZoneSetLayoutType::Columns, L"Columns");
    addTemplate(LiteZonesTypes::ZoneSetLayoutType::Grid, L"Grid");
    addTemplate(LiteZonesTypes::ZoneSetLayoutType::PriorityGrid, L"Priority Grid");

    for (const auto& [uuid, data] : CustomLayouts::instance().AllLayouts())
    {
        ListEntry entry;
        entry.isTemplate = false;
        entry.type = LiteZonesTypes::ZoneSetLayoutType::Custom;
        entry.uuid = uuid;
        entry.name = data.name;
        m_entries.push_back(entry);
    }

    SendMessageW(m_listBox, LB_RESETCONTENT, 0, 0);
    int selectIndex = 0;
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        SendMessageW(m_listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m_entries[i].name.c_str()));
        if (!IsEqualGUID(m_entries[i].uuid, GUID_NULL) && IsEqualGUID(m_entries[i].uuid, previousUuid))
        {
            selectIndex = static_cast<int>(i);
        }
    }
    SendMessageW(m_listBox, LB_SETCURSEL, selectIndex, 0);
    OnSelectionChanged();
}

void EditorWindow::PopulateMonitorCombo()
{
    SendMessageW(m_monitorCombo, CB_RESETCONTENT, 0, 0);
    m_deviceKeys.clear();
    m_deviceRects.clear();

    const std::vector<MonitorUtils::MonitorRect> monitors = MonitorUtils::GetWorkAreas(false);
    for (const auto& [monitor, rect] : monitors)
    {
        const MonitorUtils::Display display = MonitorUtils::GetDevice(monitor);
        wchar_t buffer[128]{};
        swprintf_s(buffer, L"Monitor %d (%dx%d)", display.number, rect.right - rect.left, rect.bottom - rect.top);
        SendMessageW(m_monitorCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buffer));
        m_deviceKeys.push_back(MonitorUtils::GetDeviceKey(monitor));
        m_deviceRects.push_back(rect);
    }
    SendMessageW(m_monitorCombo, CB_SETCURSEL, 0, 0);
}

bool EditorWindow::SelectedMonitorRect(RECT& out) const
{
    const int comboIndex = static_cast<int>(SendMessageW(m_monitorCombo, CB_GETCURSEL, 0, 0));
    if (comboIndex < 0 || comboIndex >= static_cast<int>(m_deviceRects.size()))
    {
        return false;
    }
    out = m_deviceRects[static_cast<size_t>(comboIndex)];
    return true;
}

int EditorWindow::SelectedListIndex() const
{
    return static_cast<int>(SendMessageW(m_listBox, LB_GETCURSEL, 0, 0));
}

void EditorWindow::OnSelectionChanged()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        EnableWindow(GetDlgItem(m_hwnd, kBtnDuplicate), FALSE);
        EnableWindow(GetDlgItem(m_hwnd, kBtnDelete), FALSE);
        EnableWindow(GetDlgItem(m_hwnd, kBtnRename), FALSE);
        if (m_spacingEdit)
        {
            EnableWindow(m_spacingEdit, FALSE);
        }
        if (m_zoneCountEdit)
        {
            EnableWindow(m_zoneCountEdit, FALSE);
        }
        if (m_staticHint)
        {
            SetWindowTextW(m_staticHint, L"");
        }
        EditorCanvas::SetZones(m_canvas, kDefaultVirtualWidth, kDefaultVirtualHeight, {});
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    const bool isCustom = !entry.isTemplate;
    EnableWindow(GetDlgItem(m_hwnd, kBtnDuplicate), FALSE);
    EnableWindow(GetDlgItem(m_hwnd, kBtnDelete), isCustom);
    EnableWindow(GetDlgItem(m_hwnd, kBtnRename), isCustom);

    UpdateSpacingControl();

    bool spacingEditable = true;
    bool zoneCountEditable = true;
    if (isCustom)
    {
        if (const auto* data = EnsureWorkingCopy(entry.uuid))
        {
            spacingEditable = data->type == LiteZonesTypes::CustomLayoutType::Grid;
        }
        zoneCountEditable = false;
    }
    else
    {
        spacingEditable = (entry.type != LiteZonesTypes::ZoneSetLayoutType::Rows &&
                           entry.type != LiteZonesTypes::ZoneSetLayoutType::Columns);
    }
    if (m_spacingEdit)
    {
        EnableWindow(m_spacingEdit, spacingEditable);
    }
    UpdateZoneCountControl();
    if (m_zoneCountEdit)
    {
        EnableWindow(m_zoneCountEdit, zoneCountEditable);
    }
    UpdateHint();

    UpdateCanvasPreview();
    UpdateApplyButtons();
    UpdateUndoState();
}

void EditorWindow::UpdateCanvasPreview()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        EditorCanvas::SetZones(m_canvas, kDefaultVirtualWidth, kDefaultVirtualHeight, {});
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    std::vector<EditorCanvas::ZoneRect> zones;

    RECT monitorRect{ 0, 0, kDefaultVirtualWidth, kDefaultVirtualHeight };
    SelectedMonitorRect(monitorRect);
    const int virtualWidth = std::max(1, static_cast<int>(monitorRect.right - monitorRect.left));
    const int virtualHeight = std::max(1, static_cast<int>(monitorRect.bottom - monitorRect.top));

    if (entry.isTemplate)
    {
        LayoutData layout;
        layout.type = entry.type;
        layout.showSpacing = DefaultValues::ShowSpacing;
        layout.spacing = m_spacingValue;
        layout.zoneCount = m_zoneCountValue;
        layout.sensitivityRadius = DefaultValues::SensitivityRadius;

        Layout renderer(layout);
        if (renderer.Init(monitorRect, nullptr))
        {
            for (const auto& [zoneIndex, zone] : renderer.Zones())
            {
                const RECT rect = zone.GetZoneRect();
                zones.push_back(EditorCanvas::ZoneRect{ rect, static_cast<int>(zoneIndex) });
            }
        }
    }
    else if (auto* data = EnsureWorkingCopy(entry.uuid))
    {
        if (data->type == LiteZonesTypes::CustomLayoutType::Canvas)
        {
            EditorCanvas::SetCanvasEdit(m_canvas, &data->canvas);
            return;
        }

        GridData::Grid grid(data->grid);
        EditorCanvas::SetGridEdit(m_canvas, std::move(grid), virtualWidth, virtualHeight);
        return;
    }

    EditorCanvas::SetZones(m_canvas, virtualWidth, virtualHeight, std::move(zones));
}

void EditorWindow::UpdateSpacingControl()
{
    if (!m_spacingEdit)
    {
        return;
    }

    int spacing = DefaultValues::Spacing;
    const int index = SelectedListIndex();
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
    {
        const ListEntry& entry = m_entries[static_cast<size_t>(index)];
        if (!entry.isTemplate)
        {
            if (const auto* data = EnsureWorkingCopy(entry.uuid))
            {
                if (data->type == LiteZonesTypes::CustomLayoutType::Grid)
                {
                    spacing = data->grid.spacing();
                }
            }
        }
    }

    m_spacingValue = spacing;
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%d", spacing);
    SetWindowTextW(m_spacingEdit, buffer);
}

void EditorWindow::OnSpacingChanged()
{
    wchar_t buffer[16]{};
    GetWindowTextW(m_spacingEdit, buffer, static_cast<int>(std::size(buffer)));
    int value = DefaultValues::Spacing;
    if (swscanf_s(buffer, L"%d", &value) != 1)
    {
        return;
    }
    value = std::max(0, std::min(kMaxSpacing, value));
    m_spacingValue = value;
    m_dirty = true;

    const int index = SelectedListIndex();
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
    {
        const ListEntry& entry = m_entries[static_cast<size_t>(index)];
        if (!entry.isTemplate)
        {
            if (auto* data = EnsureWorkingCopy(entry.uuid))
            {
                if (data->type == LiteZonesTypes::CustomLayoutType::Grid)
                {
                    data->grid.setShowSpacing(value > 0);
                    data->grid.setSpacing(value);
                }
            }
        }
    }

    UpdateCanvasPreview();
    UpdateApplyButtons();
}

void EditorWindow::UpdateZoneCountControl()
{
    if (!m_zoneCountEdit)
    {
        return;
    }

    int zoneCount = DefaultValues::ZoneCount;
    const int index = SelectedListIndex();
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
    {
        const ListEntry& entry = m_entries[static_cast<size_t>(index)];
        if (!entry.isTemplate)
        {
            if (const auto* data = EnsureWorkingCopy(entry.uuid))
            {
                zoneCount = data->type == LiteZonesTypes::CustomLayoutType::Grid
                                ? data->grid.zoneCount()
                                : static_cast<int>(data->canvas.zones.size());
            }
        }
    }

    m_zoneCountValue = zoneCount;
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%d", zoneCount);
    SetWindowTextW(m_zoneCountEdit, buffer);
}

void EditorWindow::OnZoneCountChanged()
{
    wchar_t buffer[16]{};
    GetWindowTextW(m_zoneCountEdit, buffer, static_cast<int>(std::size(buffer)));
    int value = DefaultValues::ZoneCount;
    if (swscanf_s(buffer, L"%d", &value) != 1)
    {
        return;
    }
    value = std::max(1, std::min(kMaxZoneCount, value));
    m_zoneCountValue = value;
    m_dirty = true;

    UpdateCanvasPreview();
    UpdateApplyButtons();
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

void EditorWindow::NotifyChanged()
{
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void EditorWindow::UpdateApplyButtons()
{
    const int listIndex = SelectedListIndex();
    LayoutData desired{};
    const bool hasLayout = BuildApplyLayout(listIndex, desired);

    bool canApply = false;
    bool canApplyAll = false;
    if (hasLayout)
    {
        const int comboIndex = static_cast<int>(SendMessageW(m_monitorCombo, CB_GETCURSEL, 0, 0));
        if (comboIndex >= 0 && comboIndex < static_cast<int>(m_deviceKeys.size()))
        {
            const auto applied = AppliedLayouts::instance().GetDeviceLayout(m_deviceKeys[static_cast<size_t>(comboIndex)]);
            canApply = !applied.has_value() || !(*applied == desired);
        }

        canApplyAll = true;
        for (const auto& deviceKey : m_deviceKeys)
        {
            const auto applied = AppliedLayouts::instance().GetDeviceLayout(deviceKey);
            if (applied.has_value() && *applied == desired)
            {
                canApplyAll = false;
                break;
            }
        }
    }

    EnableWindow(m_btnApply, canApply ? TRUE : FALSE);
    EnableWindow(m_btnApplyAll, canApplyAll ? TRUE : FALSE);
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

void EditorWindow::OnNewLayout()
{
    NewLayoutResult result{};
    result.grid = true;
    const INT_PTR ret = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_NEW_LAYOUT), m_hwnd,
                                        &NewLayoutDialogProc, reinterpret_cast<LPARAM>(&result));
    if (ret != IDOK)
    {
        return;
    }

    GUID uuid = GUID_NULL;
    CoCreateGuid(&uuid);

    LiteZonesTypes::CustomLayoutData data;
    data.name = result.name.empty() ? L"Custom Layout" : result.name;
    data.name = MakeUniqueName(data.name);
    if (result.grid)
    {
        data.type = LiteZonesTypes::CustomLayoutType::Grid;
        const int r = std::max(1, result.rows);
        const int c = std::max(1, result.columns);
        data.grid = LayoutHelpers::MakeGridLayout(r, c);
    }
    else
    {
        data.type = LiteZonesTypes::CustomLayoutType::Canvas;
        int width = 0;
        int height = 0;
        GetPrimaryWorkArea(width, height);
        data.canvas.lastWorkAreaWidth = width;
        data.canvas.lastWorkAreaHeight = height;
        data.canvas.zones.push_back(LiteZonesTypes::CanvasLayoutInfo::Rect{ 0, 0, width, height });
        data.canvas.sensitivityRadius = DefaultValues::SensitivityRadius;
    }

    if (!CustomLayouts::instance().AddLayout(uuid, data))
    {
        return;
    }
    PopulateLayoutList();
    m_dirty = true;
    NotifyChanged();
}

void EditorWindow::OnDuplicate()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    GUID uuid = GUID_NULL;
    CoCreateGuid(&uuid);

    if (entry.isTemplate)
    {
        LiteZonesTypes::CustomLayoutData data;
        data.name = MakeUniqueName(std::wstring(entry.name) + L" (copy)");
        data.type = LiteZonesTypes::CustomLayoutType::Grid;

        int rows = 1;
        int columns = 1;
        switch (entry.type)
        {
        case LiteZonesTypes::ZoneSetLayoutType::Rows:
            rows = 3;
            break;
        case LiteZonesTypes::ZoneSetLayoutType::Columns:
            columns = 3;
            break;
        case LiteZonesTypes::ZoneSetLayoutType::Grid:
            rows = 2;
            columns = 2;
            break;
        case LiteZonesTypes::ZoneSetLayoutType::PriorityGrid:
            rows = 2;
            columns = 3;
            break;
        default:
            break;
        }

        data.grid = LayoutHelpers::MakeGridLayout(rows, columns);

        if (!CustomLayouts::instance().AddLayout(uuid, data))
        {
            return;
        }
    }
    else
    {
        const auto* data = EnsureWorkingCopy(entry.uuid);
        if (!data)
        {
            return;
        }

        LiteZonesTypes::CustomLayoutData copy = *data;
        copy.name = MakeUniqueName(copy.name);

        if (!CustomLayouts::instance().AddLayout(uuid, copy))
        {
            return;
        }
    }

    PopulateLayoutList();
    m_dirty = true;
    NotifyChanged();
}

void EditorWindow::OnDelete()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()) || m_entries[static_cast<size_t>(index)].isTemplate)
    {
        return;
    }
    const ListEntry& entry = m_entries[static_cast<size_t>(index)];

    wchar_t message[256]{};
    swprintf_s(message, L"Delete layout \"%ls\"?", entry.name.c_str());
    if (MessageBoxW(m_hwnd, message, L"LiteZones", MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    CustomLayouts::instance().DeleteLayout(entry.uuid);
    m_workingCopies.erase(entry.uuid);
    PopulateLayoutList();
    m_dirty = true;
    NotifyChanged();
}

void EditorWindow::OnRename()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()) || m_entries[static_cast<size_t>(index)].isTemplate)
    {
        return;
    }
    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    const auto* data = EnsureWorkingCopy(entry.uuid);
    if (!data)
    {
        return;
    }

    std::wstring name = data->name;
    const INT_PTR ret = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_RENAME_LAYOUT), m_hwnd,
                                        &RenameLayoutDialogProc, reinterpret_cast<LPARAM>(&name));
    if (ret != IDOK)
    {
        return;
    }
    if (name.empty())
    {
        MessageBoxW(m_hwnd, L"Layout name cannot be empty.", L"LiteZones", MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring originalName = name;
    if (name != data->name)
    {
        name = MakeUniqueName(name);
    }

    LiteZonesTypes::CustomLayoutData updated = *data;
    updated.name = name;
    if (!CustomLayouts::instance().AddLayout(entry.uuid, updated))
    {
        return;
    }
    PopulateLayoutList();
    m_dirty = true;
    NotifyChanged();

    if (name != originalName)
    {
        wchar_t msg[256]{};
        swprintf_s(msg, L"Name was \"%ls\" (already exists), saved as \"%ls\".", originalName.c_str(), name.c_str());
        SetWindowTextW(m_staticHint, msg);
    }
}

bool EditorWindow::BuildApplyLayout(int listIndex, LayoutData& out) const
{
    if (listIndex < 0 || listIndex >= static_cast<int>(m_entries.size()))
    {
        return false;
    }
    const ListEntry& entry = m_entries[static_cast<size_t>(listIndex)];
    if (entry.isTemplate)
    {
        out = LayoutData{};
        out.type = entry.type;
        out.showSpacing = DefaultValues::ShowSpacing;
        out.spacing = m_spacingValue;
        out.zoneCount = m_zoneCountValue;
        out.sensitivityRadius = DefaultValues::SensitivityRadius;
        return true;
    }

    const auto it = m_workingCopies.find(entry.uuid);
    if (it == m_workingCopies.end())
    {
        const auto layout = CustomLayouts::instance().GetLayout(entry.uuid);
        if (!layout.has_value())
        {
            return false;
        }
        out = *layout;
        out.spacing = m_spacingValue;
        return true;
    }

    out = LayoutData{};
    out.uuid = entry.uuid;
    out.type = LiteZonesTypes::ZoneSetLayoutType::Custom;
    if (it->second.type == LiteZonesTypes::CustomLayoutType::Grid)
    {
        const auto& grid = it->second.grid;
        out.sensitivityRadius = grid.sensitivityRadius();
        out.showSpacing = grid.showSpacing();
        out.spacing = grid.spacing();
        out.zoneCount = grid.zoneCount();
    }
    else
    {
        const auto& canvas = it->second.canvas;
        out.sensitivityRadius = canvas.sensitivityRadius;
        out.zoneCount = static_cast<int>(canvas.zones.size());
    }
    out.spacing = m_spacingValue;
    return true;
}

void EditorWindow::OnApply()
{
    const int comboIndex = static_cast<int>(SendMessageW(m_monitorCombo, CB_GETCURSEL, 0, 0));
    if (comboIndex < 0 || comboIndex >= static_cast<int>(m_deviceKeys.size()))
    {
        return;
    }

    const int listIndex = SelectedListIndex();
    if (listIndex >= 0 && listIndex < static_cast<int>(m_entries.size()) && !m_entries[static_cast<size_t>(listIndex)].isTemplate)
    {
        if (LiteZonesTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
        {
            CustomLayouts::instance().AddLayout(m_entries[static_cast<size_t>(listIndex)].uuid, *data);
        }
    }

    LayoutData layout;
    if (!BuildApplyLayout(listIndex, layout))
    {
        return;
    }

    AppliedLayouts::instance().ApplyLayout(m_deviceKeys[static_cast<size_t>(comboIndex)], layout);
    AppliedLayouts::instance().SaveData();
    NotifyChanged();
    UpdateApplyButtons();

    wchar_t msg[128]{};
    swprintf_s(msg, L"Applied to Monitor %d.", comboIndex + 1);
    SetWindowTextW(m_staticHint, msg);
}

void EditorWindow::OnApplyAll()
{
    const int listIndex = SelectedListIndex();
    if (listIndex >= 0 && listIndex < static_cast<int>(m_entries.size()) && !m_entries[static_cast<size_t>(listIndex)].isTemplate)
    {
        if (LiteZonesTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
        {
            CustomLayouts::instance().AddLayout(m_entries[static_cast<size_t>(listIndex)].uuid, *data);
        }
    }

    LayoutData layout;
    if (!BuildApplyLayout(listIndex, layout))
    {
        return;
    }

    for (const auto& deviceKey : m_deviceKeys)
    {
        AppliedLayouts::instance().ApplyLayout(deviceKey, layout);
    }
    AppliedLayouts::instance().SaveData();
    NotifyChanged();
    UpdateApplyButtons();
    SetWindowTextW(m_staticHint, L"Applied to all monitors.");
}

void EditorWindow::CreateMenuBar()
{
    m_menuFile = CreateMenu();
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_NEW, L"&New Layout\tCtrl+N");
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(m_menuFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_CLOSE, L"&Close\tAlt+F4");

    m_menuEdit = CreateMenu();
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenuW(m_menuEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_RENAME, L"&Rename\tF2");
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_DELETE, L"&Delete\tDel");
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_DUPLICATE, L"&Duplicate\tCtrl+D");
    AppendMenuW(m_menuEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_SETTINGS, L"Settings...");

    HMENU menuHelp = CreateMenu();
    AppendMenuW(menuHelp, MF_STRING, IDM_HELP_ABOUT, L"&About LiteZones");

    HMENU menuBar = CreateMenu();
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<LPARAM>(m_menuFile), L"&File");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<LPARAM>(m_menuEdit), L"&Edit");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<LPARAM>(menuHelp), L"&Help");

    SetMenu(m_hwnd, menuBar);
}

void EditorWindow::OnSave()
{
    const int listIndex = SelectedListIndex();
    if (listIndex >= 0 && listIndex < static_cast<int>(m_entries.size()) && !m_entries[static_cast<size_t>(listIndex)].isTemplate)
    {
        if (LiteZonesTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
        {
            CustomLayouts::instance().AddLayout(m_entries[static_cast<size_t>(listIndex)].uuid, *data);
        }
    }
    PersistAllWorkingCopies();
    m_dirty = false;
    NotifyChanged();
    UpdateApplyButtons();
}

void EditorWindow::OnUndo()
{
    if (m_undoStack.empty())
    {
        return;
    }

    const UndoEntry entry = m_undoStack.back();
    m_undoStack.pop_back();

    // Save current state to redo stack.
    const auto* currentData = CustomLayouts::instance().GetCustomLayoutData(entry.uuid);
    if (currentData)
    {
        UndoEntry redo;
        redo.uuid = entry.uuid;
        redo.name = entry.name;
        redo.data = *currentData;
        m_redoStack.push_back(std::move(redo));
    }

    m_workingCopies[entry.uuid] = entry.data;
    CustomLayouts::instance().AddLayout(entry.uuid, entry.data);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (!m_entries[i].isTemplate && IsEqualGUID(m_entries[i].uuid, entry.uuid))
        {
            SendMessageW(m_listBox, LB_SETCURSEL, static_cast<WPARAM>(i), 0);
            OnSelectionChanged();
            break;
        }
    }
    NotifyChanged();
    UpdateUndoState();
}

void EditorWindow::OnRedo()
{
    if (m_redoStack.empty())
    {
        return;
    }

    const UndoEntry entry = m_redoStack.back();
    m_redoStack.pop_back();

    // Save current state to undo stack.
    const auto* currentData = CustomLayouts::instance().GetCustomLayoutData(entry.uuid);
    if (currentData)
    {
        UndoEntry undo;
        undo.uuid = entry.uuid;
        undo.name = entry.name;
        undo.data = *currentData;
        m_undoStack.push_back(std::move(undo));
    }

    m_workingCopies[entry.uuid] = entry.data;
    CustomLayouts::instance().AddLayout(entry.uuid, entry.data);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (!m_entries[i].isTemplate && IsEqualGUID(m_entries[i].uuid, entry.uuid))
        {
            SendMessageW(m_listBox, LB_SETCURSEL, static_cast<WPARAM>(i), 0);
            OnSelectionChanged();
            break;
        }
    }
    NotifyChanged();
    UpdateUndoState();
}

void EditorWindow::OnAbout()
{
    MessageBoxW(m_hwnd,
                L"LiteZones - Layout Editor\n\n"
                L"Create and manage custom window layouts.\n"
                L"Drag resizers to resize zones.\n"
                L"Double-click to split, right-click to merge.\n"
                L"Ctrl+Z to undo, Ctrl+Y to redo.",
                L"About LiteZones",
                MB_OK | MB_ICONINFORMATION);
}

void EditorWindow::PushUndoSnapshot()
{
    m_redoStack.clear();
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        return;
    }
    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    if (entry.isTemplate)
    {
        return;
    }
    const auto* data = CustomLayouts::instance().GetCustomLayoutData(entry.uuid);
    if (!data)
    {
        return;
    }

    UndoEntry undo;
    undo.uuid = entry.uuid;
    undo.name = entry.name;
    undo.data = *data;
    m_undoStack.push_back(std::move(undo));

    constexpr size_t kMaxUndo = 50;
    if (m_undoStack.size() > kMaxUndo)
    {
        m_undoStack.erase(m_undoStack.begin());
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
    switch (msg)
    {
    case WM_SIZE:
        LayoutControls();
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = ScaleForDpi(kMinTrackWidth, m_currentDpi);
        info->ptMinTrackSize.y = ScaleForDpi(kMinTrackHeight, m_currentDpi);
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
        if (m_dirty)
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
