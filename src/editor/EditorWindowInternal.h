#pragma once

#include "EditorWindow.h"

#include "CustomLayouts.h"
#include "LayoutHelpers.h"
#include "MonitorManager.h"
#include "resource.h"

#include <algorithm>

// Internal constants and helpers shared across EditorWindow implementation files.
namespace EditorWindowInternal
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

    struct NewLayoutResult
    {
        std::wstring name;
        bool grid = true;
        int rows = 1;
        int columns = 1;
    };

    inline std::wstring Trim(const std::wstring& text)
    {
        const size_t first = text.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos)
        {
            return {};
        }
        const size_t last = text.find_last_not_of(L" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    inline std::wstring MakeUniqueName(const std::wstring& base)
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

    inline void GetPrimaryWorkArea(int& width, int& height)
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

    inline INT_PTR CALLBACK NewLayoutDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
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
                const bool isGrid = IsDlgButtonChecked(dlg, IDC_NEW_GRID) == BST_CHECKED;
                EnableWindow(GetDlgItem(dlg, IDC_NEW_ROWS), isGrid);
                EnableWindow(GetDlgItem(dlg, IDC_NEW_COLS), isGrid);
                return TRUE;
            }
            case IDOK:
            {
                auto* result = reinterpret_cast<NewLayoutResult*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
                if (result)
                {
                    wchar_t name[128]{};
                    GetDlgItemTextW(dlg, IDC_NEW_NAME, name, static_cast<int>(std::size(name)));
                    result->name = Trim(name);
                    result->grid = IsDlgButtonChecked(dlg, IDC_NEW_GRID) == BST_CHECKED;
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

    inline INT_PTR CALLBACK RenameLayoutDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
        {
            SetWindowLongPtrW(dlg, GWLP_USERDATA, lParam);
            const auto* name = reinterpret_cast<const std::wstring*>(lParam);
            if (name)
            {
                SetDlgItemTextW(dlg, IDC_RENAME_NAME, name->c_str());
                HWND edit = GetDlgItem(dlg, IDC_RENAME_NAME);
                SendMessageW(edit, EM_SETSEL, 0, -1);
                SetFocus(edit);
            }
            return FALSE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDOK:
            {
                auto* name = reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
                if (name)
                {
                    wchar_t buffer[128]{};
                    GetDlgItemTextW(dlg, IDC_RENAME_NAME, buffer, static_cast<int>(std::size(buffer)));
                    *name = Trim(buffer);
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
}
