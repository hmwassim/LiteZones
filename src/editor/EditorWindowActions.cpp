#include "EditorWindowInternal.h"

#include "AppliedLayouts.h"
#include "CustomLayouts.h"
#include "LayoutEngine.h"

#include <objbase.h>

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

void EditorWindow::OnNewLayout()
{
    using namespace EditorWindowInternal;

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
    PushCreateUndoSnapshot();
    NotifyChanged();
}

void EditorWindow::OnDuplicate()
{
    using namespace EditorWindowInternal;

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
    PushCreateUndoSnapshot();
    NotifyChanged();
}

void EditorWindow::OnDelete()
{
    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()) || m_entries[static_cast<size_t>(index)].isTemplate)
    {
        return;
    }

    PushStructuralUndoSnapshot();

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
    using namespace EditorWindowInternal;

    const int index = SelectedListIndex();
    if (index < 0 || index >= static_cast<int>(m_entries.size()) || m_entries[static_cast<size_t>(index)].isTemplate)
    {
        return;
    }

    PushStructuralUndoSnapshot();

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
    NotifyChanged();

    if (name != originalName)
    {
        wchar_t msg[256]{};
        swprintf_s(msg, L"Name was \"%ls\" (already exists), saved as \"%ls\".", originalName.c_str(), name.c_str());
        SetWindowTextW(m_staticHint, msg);
    }
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

    if (entry.kind == UndoKind::WorkingCopyEdit)
    {
        const auto* currentData = EnsureWorkingCopy(entry.uuid);
        if (currentData)
        {
            UndoEntry redo;
            redo.kind = UndoKind::WorkingCopyEdit;
            redo.uuid = entry.uuid;
            redo.name = entry.name;
            redo.data = *currentData;
            m_redoStack.push_back(std::move(redo));
        }

        m_workingCopies[entry.uuid] = entry.data;
    }
    else
    {
        const auto* currentData = CustomLayouts::instance().GetCustomLayoutData(entry.uuid);
        if (currentData)
        {
            UndoEntry redo;
            redo.kind = UndoKind::StructuralChange;
            redo.uuid = entry.uuid;
            redo.name = entry.name;
            redo.data = *currentData;
            m_redoStack.push_back(std::move(redo));
        }

        m_workingCopies[entry.uuid] = entry.data;
        CustomLayouts::instance().AddLayout(entry.uuid, entry.data);
        PopulateLayoutList();
    }

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

    if (entry.kind == UndoKind::WorkingCopyEdit)
    {
        const auto* currentData = EnsureWorkingCopy(entry.uuid);
        if (currentData)
        {
            UndoEntry undo;
            undo.kind = UndoKind::WorkingCopyEdit;
            undo.uuid = entry.uuid;
            undo.name = entry.name;
            undo.data = *currentData;
            m_undoStack.push_back(std::move(undo));
        }

        m_workingCopies[entry.uuid] = entry.data;
    }
    else
    {
        const auto* currentData = CustomLayouts::instance().GetCustomLayoutData(entry.uuid);
        if (currentData)
        {
            UndoEntry undo;
            undo.kind = UndoKind::StructuralChange;
            undo.uuid = entry.uuid;
            undo.name = entry.name;
            undo.data = *currentData;
            m_undoStack.push_back(std::move(undo));
        }

        m_workingCopies[entry.uuid] = entry.data;
        CustomLayouts::instance().AddLayout(entry.uuid, entry.data);
        PopulateLayoutList();
    }

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
    const auto* data = EnsureWorkingCopy(entry.uuid);
    if (!data)
    {
        return;
    }

    UndoEntry undo;
    undo.kind = UndoKind::WorkingCopyEdit;
    undo.uuid = entry.uuid;
    undo.name = entry.name;
    undo.data = *data;
    m_undoStack.push_back(std::move(undo));

    constexpr size_t kMaxUndo = 50;
    if (m_undoStack.size() > kMaxUndo)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    UpdateUndoState();
}

void EditorWindow::PushCreateUndoSnapshot()
{
    m_redoStack.clear();
    constexpr size_t kMaxUndo = 50;
    if (m_undoStack.size() > kMaxUndo)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    UpdateUndoState();
}

void EditorWindow::PushStructuralUndoSnapshot()
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
    const auto* data = EnsureWorkingCopy(entry.uuid);
    if (!data)
    {
        return;
    }

    UndoEntry undo;
    undo.kind = UndoKind::StructuralChange;
    undo.uuid = entry.uuid;
    undo.name = entry.name;
    undo.data = *data;
    m_undoStack.push_back(std::move(undo));

    constexpr size_t kMaxUndo = 50;
    if (m_undoStack.size() > kMaxUndo)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    UpdateUndoState();
}
