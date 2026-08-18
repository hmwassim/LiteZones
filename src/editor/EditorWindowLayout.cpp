#include "EditorWindowInternal.h"

#include "AppliedLayouts.h"
#include "EditorCanvas.h"
#include "GridData.h"
#include "LayoutEngine.h"

bool EditorWindow::CreateControls()
{
    using namespace EditorWindowInternal;

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

    EditorCanvas::SetOnEdited(m_canvas, [this]() { NotifyChanged(); UpdateApplyButtons(); });
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
    using namespace EditorWindowInternal;

    if (!m_hwnd)
    {
        return;
    }
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const UINT dpi = m_currentDpi;

    const int panelW = LayoutHelpers::ScaleForDpi(kLeftPanelWidth, dpi);
    const int pad = LayoutHelpers::ScaleForDpi(8, dpi);
    const int ctrlH = LayoutHelpers::ScaleForDpi(24, dpi);
    const int labelH = LayoutHelpers::ScaleForDpi(16, dpi);
    const int listHeight = std::max(LayoutHelpers::ScaleForDpi(120, dpi), height - pad - LayoutHelpers::ScaleForDpi(64, dpi));
    MoveWindow(m_listBox, pad, pad, panelW - pad * 2, listHeight, TRUE);
    MoveWindow(m_staticMonitor, panelW + pad, LayoutHelpers::ScaleForDpi(13, dpi), LayoutHelpers::ScaleForDpi(56, dpi), labelH, TRUE);
    MoveWindow(m_monitorCombo, panelW + LayoutHelpers::ScaleForDpi(66, dpi), LayoutHelpers::ScaleForDpi(10, dpi), LayoutHelpers::ScaleForDpi(170, dpi), LayoutHelpers::ScaleForDpi(200, dpi), TRUE);
    MoveWindow(m_staticSpacing, panelW + LayoutHelpers::ScaleForDpi(244, dpi), LayoutHelpers::ScaleForDpi(13, dpi), LayoutHelpers::ScaleForDpi(48, dpi), labelH, TRUE);
    MoveWindow(m_spacingEdit, panelW + LayoutHelpers::ScaleForDpi(296, dpi), LayoutHelpers::ScaleForDpi(10, dpi), LayoutHelpers::ScaleForDpi(44, dpi), ctrlH, TRUE);
    MoveWindow(m_staticZones, panelW + LayoutHelpers::ScaleForDpi(348, dpi), LayoutHelpers::ScaleForDpi(13, dpi), LayoutHelpers::ScaleForDpi(40, dpi), labelH, TRUE);
    MoveWindow(m_zoneCountEdit, panelW + LayoutHelpers::ScaleForDpi(390, dpi), LayoutHelpers::ScaleForDpi(10, dpi), LayoutHelpers::ScaleForDpi(40, dpi), ctrlH, TRUE);
    MoveWindow(m_btnApply, panelW + LayoutHelpers::ScaleForDpi(438, dpi), LayoutHelpers::ScaleForDpi(10, dpi), LayoutHelpers::ScaleForDpi(56, dpi), ctrlH, TRUE);
    MoveWindow(m_btnApplyAll, panelW + LayoutHelpers::ScaleForDpi(500, dpi), LayoutHelpers::ScaleForDpi(10, dpi), LayoutHelpers::ScaleForDpi(72, dpi), ctrlH, TRUE);

    const int buttonWidth = (panelW - pad * 2 - pad) / 2;
    MoveWindow(GetDlgItem(m_hwnd, kBtnNew), pad, height - LayoutHelpers::ScaleForDpi(60, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDuplicate), pad + buttonWidth + pad, height - LayoutHelpers::ScaleForDpi(60, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnDelete), pad, height - LayoutHelpers::ScaleForDpi(32, dpi), buttonWidth, ctrlH, TRUE);
    MoveWindow(GetDlgItem(m_hwnd, kBtnRename), pad + buttonWidth + pad, height - LayoutHelpers::ScaleForDpi(32, dpi), buttonWidth, ctrlH, TRUE);

    MoveWindow(m_canvas, panelW + pad, LayoutHelpers::ScaleForDpi(40, dpi), std::max(LayoutHelpers::ScaleForDpi(60, dpi), width - panelW - pad * 2), std::max(LayoutHelpers::ScaleForDpi(60, dpi), height - LayoutHelpers::ScaleForDpi(40, dpi) - LayoutHelpers::ScaleForDpi(44, dpi)), TRUE);
    MoveWindow(m_staticHint, panelW + pad, height - LayoutHelpers::ScaleForDpi(38, dpi), std::max(LayoutHelpers::ScaleForDpi(60, dpi), width - panelW - pad * 2), LayoutHelpers::ScaleForDpi(36, dpi), TRUE);
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
    m_selectedIndex = selectIndex;
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
    using namespace EditorWindowInternal;

    const int index = SelectedListIndex();

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size()) && index != m_selectedIndex)
    {
        const ListEntry& prev = m_entries[static_cast<size_t>(m_selectedIndex)];
        if (!prev.isTemplate)
        {
            DiscardWorkingCopy(prev.uuid);
        }
    }
    m_selectedIndex = index;

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
    using namespace EditorWindowInternal;

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
        EditorCanvas::SetGridEdit(m_canvas, std::move(grid), virtualWidth, virtualHeight, m_spacingValue);
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
        else
        {
            const int comboIndex = static_cast<int>(SendMessageW(m_monitorCombo, CB_GETCURSEL, 0, 0));
            if (comboIndex >= 0 && comboIndex < static_cast<int>(m_deviceKeys.size()))
            {
                const auto applied = AppliedLayouts::instance().GetDeviceLayout(m_deviceKeys[static_cast<size_t>(comboIndex)]);
                if (applied.has_value() && applied->type == entry.type)
                {
                    spacing = applied->spacing;
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
    using namespace EditorWindowInternal;

    wchar_t buffer[16]{};
    GetWindowTextW(m_spacingEdit, buffer, static_cast<int>(std::size(buffer)));
    int value = DefaultValues::Spacing;
    if (swscanf_s(buffer, L"%d", &value) != 1)
    {
        return;
    }
    value = std::max(0, std::min(kMaxSpacing, value));
    if (value == m_spacingValue)
    {
        return;
    }
    PushUndoSnapshot();
    m_spacingValue = value;

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
        else
        {
            const int comboIndex = static_cast<int>(SendMessageW(m_monitorCombo, CB_GETCURSEL, 0, 0));
            if (comboIndex >= 0 && comboIndex < static_cast<int>(m_deviceKeys.size()))
            {
                const auto applied = AppliedLayouts::instance().GetDeviceLayout(m_deviceKeys[static_cast<size_t>(comboIndex)]);
                if (applied.has_value() && applied->type == entry.type)
                {
                    zoneCount = applied->zoneCount;
                }
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
    using namespace EditorWindowInternal;

    wchar_t buffer[16]{};
    GetWindowTextW(m_zoneCountEdit, buffer, static_cast<int>(std::size(buffer)));
    int value = DefaultValues::ZoneCount;
    if (swscanf_s(buffer, L"%d", &value) != 1)
    {
        return;
    }
    value = std::max(1, std::min(kMaxZoneCount, value));
    if (value == m_zoneCountValue)
    {
        return;
    }
    PushUndoSnapshot();
    m_zoneCountValue = value;

    UpdateCanvasPreview();
    UpdateApplyButtons();
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
        const bool isCustom = listIndex >= 0 && listIndex < static_cast<int>(m_entries.size()) &&
                              !m_entries[static_cast<size_t>(listIndex)].isTemplate;
        if (isCustom)
        {
            const bool dirty = WorkingCopyDiffersFromStore();
            canApply = dirty;
            canApplyAll = dirty;
        }
        else
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
    }

    EnableWindow(m_btnApply, canApply ? TRUE : FALSE);
    EnableWindow(m_btnApplyAll, canApplyAll ? TRUE : FALSE);
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
