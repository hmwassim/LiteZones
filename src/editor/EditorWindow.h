#pragma once

#include "LayoutTypes.h"
#include "GuidUtils.h"

#include <windows.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

struct SettingsData;

class EditorWindow
{
public:
    EditorWindow(HINSTANCE hInstance, HWND notifyWindow, const SettingsData& settings);
    ~EditorWindow();

    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    bool Create();
    void Show();
    void Close();

    HWND Hwnd() const { return m_hwnd; }
    HACCEL GetAccel() const { return m_hAccel; }
    bool IsOpen() const { return m_hwnd != nullptr; }
    bool IsEditFocused() const;

    // Called after any change that persists to the stores (layout added/deleted,
    // layout applied) so the runtime can rebuild work areas.
    void SetOnChanged(std::function<void()> callback) { m_onChanged = std::move(callback); }

private:
    struct ListEntry
    {
        bool isTemplate = false;
        LiteZonesTypes::ZoneSetLayoutType type = LiteZonesTypes::ZoneSetLayoutType::Rows;
        GUID uuid = GUID_NULL;
        std::wstring name;
    };

    enum ControlId
    {
        kListLayouts = 5001,
        kMonitorCombo = 5002,
        kBtnNew = 5003,
        kBtnDuplicate = 5004,
        kBtnDelete = 5005,
        kBtnRename = 5006,
        kBtnApply = 5007,
        kBtnApplyAll = 5008,
        kEditSpacing = 5009,
        kEditZoneCount = 5010,
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateControls();
    void CreateMenuBar();
    void LayoutControls();
    void PopulateLayoutList();
    void PopulateMonitorCombo();
    void OnSelectionChanged();
    void UpdateCanvasPreview();
    void UpdateSpacingControl();
    void OnSpacingChanged();
    void UpdateZoneCountControl();
    void OnZoneCountChanged();
    void UpdateHint();
    void UpdateApplyButtons();
    void UpdateUndoState();
    void NotifyChanged();

    void OnNewLayout();
    void OnDuplicate();
    void OnDelete();
    void OnRename();
    void OnSave();
    void OnApply();
    void OnApplyAll();
    void OnUndo();
    void OnRedo();
    void OnAbout();

    int SelectedListIndex() const;
    bool BuildApplyLayout(int listIndex, LayoutData& out) const;
    bool SelectedMonitorRect(RECT& out) const;
    LiteZonesTypes::CustomLayoutData* EnsureWorkingCopy(const GUID& uuid);
    void DiscardWorkingCopy(const GUID& uuid);
    void PersistAllWorkingCopies();
    void PushUndoSnapshot();
    void PushCreateUndoSnapshot();
    void PushStructuralUndoSnapshot();
    bool WorkingCopyDiffersFromStore() const;
    bool HasUnsavedChanges() const;
    void SelectActiveLayout();

    HINSTANCE m_hInstance = nullptr;
    HWND m_notifyWindow = nullptr;
    const SettingsData& m_settings;
    HWND m_hwnd = nullptr;
    HWND m_listBox = nullptr;
    HWND m_staticMonitor = nullptr;
    HWND m_monitorCombo = nullptr;
    HWND m_staticSpacing = nullptr;
    HWND m_spacingEdit = nullptr;
    HWND m_staticZones = nullptr;
    HWND m_zoneCountEdit = nullptr;
    HWND m_staticHint = nullptr;
    HWND m_canvas = nullptr;
    HWND m_btnApply = nullptr;
    HWND m_btnApplyAll = nullptr;
    int m_spacingValue = DefaultValues::Spacing;
    int m_zoneCountValue = DefaultValues::ZoneCount;
    std::vector<ListEntry> m_entries;
    std::vector<std::wstring> m_deviceKeys;
    std::vector<RECT> m_deviceRects;
    std::map<GUID, LiteZonesTypes::CustomLayoutData, Util::GuidLess> m_workingCopies;
    std::function<void()> m_onChanged;
    // m_dirty removed — use HasUnsavedChanges() instead.
    int m_selectedIndex = -1;
    HMENU m_menuFile = nullptr;
    HMENU m_menuEdit = nullptr;
    HACCEL m_hAccel = nullptr;
    UINT m_currentDpi = 96;

    enum class UndoKind
    {
        WorkingCopyEdit,
        StructuralChange
    };

    struct UndoEntry
    {
        UndoKind kind = UndoKind::WorkingCopyEdit;
        GUID uuid = GUID_NULL;
        std::wstring name;
        LiteZonesTypes::CustomLayoutData data;
    };
    std::vector<UndoEntry> m_undoStack;
    std::vector<UndoEntry> m_redoStack;
};
