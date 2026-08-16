#pragma once

#include "LayoutTypes.h"
#include "util.h"

#include <windows.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

// In-process layout editor: a non-modal window listing built-in templates and
// custom layouts, with a zone preview and per-monitor apply. Reads and writes
// the same custom-layouts.json / applied-layouts.json stores the runtime uses.
class EditorWindow
{
public:
    EditorWindow(HINSTANCE hInstance, HWND notifyWindow);
    ~EditorWindow() = default;

    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    bool Create();
    void Show();
    void Close();

    HWND Hwnd() const { return m_hwnd; }
    bool IsOpen() const { return m_hwnd != nullptr; }

    // Called after any change that persists to the stores (layout added/deleted,
    // layout applied) so the runtime can rebuild work areas.
    void SetOnChanged(std::function<void()> callback) { m_onChanged = std::move(callback); }

private:
    struct ListEntry
    {
        bool isTemplate = false;
        FancyZonesDataTypes::ZoneSetLayoutType type = FancyZonesDataTypes::ZoneSetLayoutType::Blank;
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
    void NotifyChanged();

    void OnNewLayout();
    void OnDuplicate();
    void OnDelete();
    void OnRename();
    void OnApply();
    void OnApplyAll();

    int SelectedListIndex() const;
    bool BuildApplyLayout(int listIndex, LayoutData& out) const;
    bool SelectedMonitorRect(RECT& out) const;
    FancyZonesDataTypes::CustomLayoutData* EnsureWorkingCopy(const GUID& uuid);
    void PersistAllWorkingCopies();

    HINSTANCE m_hInstance = nullptr;
    HWND m_notifyWindow = nullptr;
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
    int m_spacingValue = DefaultValues::Spacing;
    int m_zoneCountValue = DefaultValues::ZoneCount;
    std::vector<ListEntry> m_entries;
    std::vector<std::wstring> m_deviceKeys;
    std::vector<RECT> m_deviceRects;
    std::map<GUID, FancyZonesDataTypes::CustomLayoutData, Util::GuidLess> m_workingCopies;
    std::function<void()> m_onChanged;
};
