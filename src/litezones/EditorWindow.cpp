#include "EditorWindow.h"

#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "EditorCanvas.h"
#include "GridData.h"
#include "LayoutEngine.h"
#include "MonitorManager.h"
#include "resource.h"
#include "util.h"

#include <objbase.h>

#include <algorithm>

namespace
{
    constexpr wchar_t kEditorClassName[] = L"LiteZonesEditorWindow";
    constexpr wchar_t kWindowTitle[] = L"LiteZones - Layout Editor";

    constexpr int kLeftPanelWidth = 216;

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
        width = 1600;
        height = 900;
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
            SetWindowLongPtrW(dlg, GWLP_USERDATA, lParam);
            CheckDlgButton(dlg, IDC_NEW_GRID, BST_CHECKED);
            SetDlgItemInt(dlg, IDC_NEW_ROWS, 2, FALSE);
            SetDlgItemInt(dlg, IDC_NEW_COLS, 2, FALSE);
            SetFocus(GetDlgItem(dlg, IDC_NEW_NAME));
            return TRUE;

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
                    result->rows = std::max(1, static_cast<int>(GetDlgItemInt(dlg, IDC_NEW_ROWS, nullptr, FALSE)));
                    result->columns = std::max(1, static_cast<int>(GetDlgItemInt(dlg, IDC_NEW_COLS, nullptr, FALSE)));
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

EditorWindow::EditorWindow(HINSTANCE hInstance, HWND notifyWindow) :
    m_hInstance(hInstance),
    m_notifyWindow(notifyWindow)
{
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

    m_hwnd = CreateWindowExW(WS_CLIPCHILDREN, kEditorClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 900, 620, nullptr, nullptr, m_hInstance, this);
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
    return true;
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
    if (!m_listBox || !m_monitorCombo || !m_canvas)
    {
        return false;
    }

    EditorCanvas::SetOnEdited(m_canvas, [this]() { NotifyChanged(); });
    EditorCanvas::SetOnBeforeEdit(m_canvas, [this]() { PushUndoSnapshot(); });

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

    for (HWND control : { m_listBox, m_staticMonitor, m_monitorCombo, m_staticSpacing, m_spacingEdit,
                          m_staticZones, m_zoneCountEdit, m_staticHint, m_canvas })
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

    const int listHeight = std::max(120, height - 16 - 64);
    MoveWindow(m_listBox, 8, 8, kLeftPanelWidth - 16, listHeight, TRUE);
    MoveWindow(m_staticMonitor, kLeftPanelWidth + 8, 13, 56, 16, TRUE);
    MoveWindow(m_monitorCombo, kLeftPanelWidth + 66, 10, 170, 200, TRUE);
    MoveWindow(m_staticSpacing, kLeftPanelWidth + 244, 13, 48, 16, TRUE);
    MoveWindow(m_spacingEdit, kLeftPanelWidth + 296, 10, 44, 24, TRUE);
    MoveWindow(m_staticZones, kLeftPanelWidth + 348, 13, 40, 16, TRUE);
    MoveWindow(m_zoneCountEdit, kLeftPanelWidth + 390, 10, 40, 24, TRUE);

    const int buttonWidth = (kLeftPanelWidth - 16 - 8) / 2;
    MoveWindow(GetDlgItem(m_hwnd, kBtnNew), 8, height - 60, buttonWidth, 24, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDuplicate), 8 + buttonWidth + 8, height - 60, buttonWidth, 24, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDelete), 8, height - 32, buttonWidth, 24, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnRename), 8 + buttonWidth + 8, height - 32, buttonWidth, 24, TRUE);

    MoveWindow(m_canvas, kLeftPanelWidth + 8, 40, std::max(60, width - kLeftPanelWidth - 16), std::max(60, height - 40 - 32), TRUE);
    MoveWindow(m_staticHint, kLeftPanelWidth + 8, height - 24, std::max(60, width - kLeftPanelWidth - 16), 18, TRUE);
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
    const auto addTemplate = [this](FancyZonesDataTypes::ZoneSetLayoutType type, const wchar_t* name) {
        ListEntry entry;
        entry.isTemplate = true;
        entry.type = type;
        entry.uuid = GUID_NULL;
        entry.name = name;
        m_entries.push_back(entry);
    };
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Focus, L"Focus");
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Rows, L"Rows");
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Columns, L"Columns");
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Grid, L"Grid");
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid, L"Priority Grid");
    addTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Blank, L"Blank");

    for (const auto& [uuid, data] : CustomLayouts::instance().AllLayouts())
    {
        ListEntry entry;
        entry.isTemplate = false;
        entry.type = FancyZonesDataTypes::ZoneSetLayoutType::Custom;
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
        EditorCanvas::SetZones(m_canvas, 1600, 900, {});
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    const bool isCustom = !entry.isTemplate;
    EnableWindow(GetDlgItem(m_hwnd, kBtnDuplicate), TRUE);
    EnableWindow(GetDlgItem(m_hwnd, kBtnDelete), isCustom);
    EnableWindow(GetDlgItem(m_hwnd, kBtnRename), isCustom);

    UpdateSpacingControl();

    bool spacingEditable = true;
    bool zoneCountEditable = true;
    if (isCustom)
    {
        if (const auto* data = EnsureWorkingCopy(entry.uuid))
        {
            spacingEditable = data->type == FancyZonesDataTypes::CustomLayoutType::Grid;
        }
        zoneCountEditable = false;
    }
    else if (entry.type == FancyZonesDataTypes::ZoneSetLayoutType::Blank)
    {
        zoneCountEditable = false;
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
}

void EditorWindow::UpdateCanvasPreview()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        EditorCanvas::SetZones(m_canvas, 1600, 900, {});
        return;
    }

    const ListEntry& entry = m_entries[static_cast<size_t>(index)];
    std::vector<EditorCanvas::ZoneRect> zones;

    RECT monitorRect{ 0, 0, 1600, 900 };
    SelectedMonitorRect(monitorRect);
    const int virtualWidth = std::max(1, static_cast<int>(monitorRect.right - monitorRect.left));
    const int virtualHeight = std::max(1, static_cast<int>(monitorRect.bottom - monitorRect.top));

    if (entry.isTemplate)
    {
        if (entry.type != FancyZonesDataTypes::ZoneSetLayoutType::Blank)
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
    }
    else if (auto* data = EnsureWorkingCopy(entry.uuid))
    {
        if (data->type == FancyZonesDataTypes::CustomLayoutType::Canvas)
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
                if (data->type == FancyZonesDataTypes::CustomLayoutType::Grid)
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
    value = std::max(0, std::min(100, value));
    m_spacingValue = value;

    const int index = SelectedListIndex();
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
    {
        const ListEntry& entry = m_entries[static_cast<size_t>(index)];
        if (!entry.isTemplate)
        {
            if (auto* data = EnsureWorkingCopy(entry.uuid))
            {
                if (data->type == FancyZonesDataTypes::CustomLayoutType::Grid)
                {
                    data->grid.m_showSpacing = value > 0;
                    data->grid.m_spacing = value;
                }
            }
        }
    }

    UpdateCanvasPreview();
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
                zoneCount = data->type == FancyZonesDataTypes::CustomLayoutType::Grid
                                ? data->grid.zoneCount()
                                : static_cast<int>(data->canvas.zones.size());
            }
        }
        else if (entry.type == FancyZonesDataTypes::ZoneSetLayoutType::Blank)
        {
            zoneCount = 0;
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
    value = std::max(1, std::min(16, value));
    m_zoneCountValue = value;

    UpdateCanvasPreview();
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
            if (data->type == FancyZonesDataTypes::CustomLayoutType::Grid)
            {
                SetWindowTextW(m_staticHint,
                               L"Grid: drag separators to resize; double-click to split; "
                               L"Ctrl+click zones to multi-select, then right-click to merge.");
            }
            else
            {
                SetWindowTextW(m_staticHint,
                               L"Canvas: drag empty space to draw a zone, drag a zone to move it, drag a handle to "
                               L"resize it; right-click a zone to delete it.");
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

FancyZonesDataTypes::CustomLayoutData* EditorWindow::EnsureWorkingCopy(const GUID& uuid)
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

    FancyZonesDataTypes::CustomLayoutData data;
    data.name = result.name.empty() ? L"Custom Layout" : result.name;
    data.name = MakeUniqueName(data.name);
    if (result.grid)
    {
        data.type = FancyZonesDataTypes::CustomLayoutType::Grid;
        const int r = std::max(1, result.rows);
        const int c = std::max(1, result.columns);
        data.grid = FancyZonesDataTypes::GridLayoutInfo(r, c);
        const int rowPct = 10000 / r;
        const int colPct = 10000 / c;
        for (int i = 0; i < r; ++i)
        {
            data.grid.rowsPercents()[i] = (i < r - 1) ? rowPct : 10000 - rowPct * (r - 1);
        }
        for (int i = 0; i < c; ++i)
        {
            data.grid.columnsPercents()[i] = (i < c - 1) ? colPct : 10000 - colPct * (c - 1);
        }
        int zoneIndex = 0;
        data.grid.cellChildMap().resize(r);
        for (int row = 0; row < r; ++row)
        {
            data.grid.cellChildMap()[row].resize(c);
            for (int col = 0; col < c; ++col)
            {
                data.grid.cellChildMap()[row][col] = zoneIndex++;
            }
        }
        data.grid.m_showSpacing = DefaultValues::ShowSpacing;
        data.grid.m_spacing = DefaultValues::Spacing;
        data.grid.m_sensitivityRadius = DefaultValues::SensitivityRadius;
    }
    else
    {
        data.type = FancyZonesDataTypes::CustomLayoutType::Canvas;
        int width = 0;
        int height = 0;
        GetPrimaryWorkArea(width, height);
        data.canvas.lastWorkAreaWidth = width;
        data.canvas.lastWorkAreaHeight = height;
        data.canvas.zones.push_back(FancyZonesDataTypes::CanvasLayoutInfo::Rect{ 0, 0, width, height });
        data.canvas.sensitivityRadius = DefaultValues::SensitivityRadius;
    }

    if (!CustomLayouts::instance().AddLayout(uuid, data))
    {
        return;
    }
    PopulateLayoutList();
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
        FancyZonesDataTypes::CustomLayoutData data;
        data.name = MakeUniqueName(std::wstring(entry.name) + L" (copy)");
        data.type = FancyZonesDataTypes::CustomLayoutType::Grid;

        int rows = 1;
        int columns = 1;
        switch (entry.type)
        {
        case FancyZonesDataTypes::ZoneSetLayoutType::Focus:
            columns = 3;
            break;
        case FancyZonesDataTypes::ZoneSetLayoutType::Rows:
            rows = 3;
            break;
        case FancyZonesDataTypes::ZoneSetLayoutType::Columns:
            columns = 3;
            break;
        case FancyZonesDataTypes::ZoneSetLayoutType::Grid:
            rows = 2;
            columns = 2;
            break;
        case FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid:
            rows = 2;
            columns = 3;
            break;
        default:
            break;
        }

        data.grid = FancyZonesDataTypes::GridLayoutInfo(rows, columns);
        const int rowPct = 10000 / rows;
        const int colPct = 10000 / columns;
        for (int i = 0; i < rows; ++i)
        {
            data.grid.rowsPercents()[i] = (i < rows - 1) ? rowPct : 10000 - rowPct * (rows - 1);
        }
        for (int i = 0; i < columns; ++i)
        {
            data.grid.columnsPercents()[i] = (i < columns - 1) ? colPct : 10000 - colPct * (columns - 1);
        }
        int zoneIndex = 0;
        data.grid.cellChildMap().resize(rows);
        for (int row = 0; row < rows; ++row)
        {
            data.grid.cellChildMap()[row].resize(columns);
            for (int col = 0; col < columns; ++col)
            {
                data.grid.cellChildMap()[row][col] = zoneIndex++;
            }
        }
        data.grid.m_showSpacing = DefaultValues::ShowSpacing;
        data.grid.m_spacing = DefaultValues::Spacing;
        data.grid.m_sensitivityRadius = DefaultValues::SensitivityRadius;

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

        FancyZonesDataTypes::CustomLayoutData copy = *data;
        copy.name = MakeUniqueName(copy.name);

        if (!CustomLayouts::instance().AddLayout(uuid, copy))
        {
            return;
        }
    }

    PopulateLayoutList();
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
    if (ret != IDOK || name.empty())
    {
        return;
    }

    FancyZonesDataTypes::CustomLayoutData updated = *data;
    updated.name = name;
    if (!CustomLayouts::instance().AddLayout(entry.uuid, updated))
    {
        return;
    }
    PopulateLayoutList();
    NotifyChanged();
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
        out.zoneCount = (entry.type == FancyZonesDataTypes::ZoneSetLayoutType::Blank) ? 0 : m_zoneCountValue;
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
    out.type = FancyZonesDataTypes::ZoneSetLayoutType::Custom;
    if (it->second.type == FancyZonesDataTypes::CustomLayoutType::Grid)
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
        if (FancyZonesDataTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
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
}

void EditorWindow::OnApplyAll()
{
    const int listIndex = SelectedListIndex();
    if (listIndex >= 0 && listIndex < static_cast<int>(m_entries.size()) && !m_entries[static_cast<size_t>(listIndex)].isTemplate)
    {
        if (FancyZonesDataTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
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
}

void EditorWindow::CreateMenuBar()
{
    m_menuFile = CreateMenu();
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(m_menuFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_APPLY, L"Apply &to Monitor\tCtrl+Enter");
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_APPLYALL, L"Apply to &All Monitors\tCtrl+Shift+Enter");
    AppendMenuW(m_menuFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_menuFile, MF_STRING, IDM_FILE_CLOSE, L"&Close\tAlt+F4");

    m_menuEdit = CreateMenu();
    AppendMenuW(m_menuEdit, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");

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
        if (FancyZonesDataTypes::CustomLayoutData* data = EnsureWorkingCopy(m_entries[static_cast<size_t>(listIndex)].uuid))
        {
            CustomLayouts::instance().AddLayout(m_entries[static_cast<size_t>(listIndex)].uuid, *data);
        }
    }
    PersistAllWorkingCopies();
    NotifyChanged();
}

void EditorWindow::OnUndo()
{
    if (m_undoStack.empty())
    {
        return;
    }

    const UndoEntry entry = m_undoStack.back();
    m_undoStack.pop_back();

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
}

void EditorWindow::OnAbout()
{
    MessageBoxW(m_hwnd,
                L"LiteZones - Layout Editor\n\n"
                L"Create and manage custom window layouts.\n"
                L"Drag resizers to resize zones.\n"
                L"Double-click to split, right-click to merge.\n"
                L"Ctrl+Z to undo changes.",
                L"About LiteZones",
                MB_OK | MB_ICONINFORMATION);
}

void EditorWindow::PushUndoSnapshot()
{
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
                    if (applied.type == FancyZonesDataTypes::ZoneSetLayoutType::Custom)
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
        info->ptMinTrackSize.x = 700;
        info->ptMinTrackSize.y = 480;
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            OnUndo();
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
