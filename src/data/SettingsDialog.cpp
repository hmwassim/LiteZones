#include "SettingsDialog.h"

#include "Settings.h"
#include "AutostartUtils.h"
#include "resource.h"

#include <algorithm>
#include <vector>

namespace
{
    struct DialogContext
    {
        SettingsData edited;
    };

    void Check(HWND dlg, int id, bool value)
    {
        CheckDlgButton(dlg, id, value ? BST_CHECKED : BST_UNCHECKED);
    }

    bool IsChecked(HWND dlg, int id)
    {
        return IsDlgButtonChecked(dlg, id) == BST_CHECKED;
    }

    void SetEditText(HWND dlg, int id, const std::wstring& text)
    {
        SetDlgItemTextW(dlg, id, text.c_str());
    }

    std::wstring GetEditText(HWND dlg, int id)
    {
        wchar_t buf[128]{};
        GetDlgItemTextW(dlg, id, buf, static_cast<int>(std::size(buf)));
        return buf;
    }

    void PopulateExcludedApps(HWND dlg, const std::vector<std::wstring>& apps)
    {
        HWND list = GetDlgItem(dlg, IDC_LST_EXCLUDED_APPS);
        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (const auto& app : apps)
        {
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(app.c_str()));
        }
    }

    INT_PTR CALLBACK SettingsDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
        {
            SetWindowLongPtrW(dlg, GWLP_USERDATA, lParam);
            auto* ctx = reinterpret_cast<DialogContext*>(lParam);

            Check(dlg, IDC_CHK_SHIFT_DRAG, ctx->edited.shiftDrag);
            Check(dlg, IDC_CHK_MOUSE_SWITCH, ctx->edited.mouseSwitch);
            Check(dlg, IDC_CHK_MOUSE_MIDDLE, ctx->edited.mouseMiddleClickSpanningMultipleZones);
            Check(dlg, IDC_CHK_MOVE_ACROSS, ctx->edited.moveWindowAcrossMonitors);
            Check(dlg, IDC_CHK_RESTORE_SIZE, ctx->edited.restoreSize);
            Check(dlg, IDC_CHK_SPAN_MONITORS, ctx->edited.spanZonesAcrossMonitors);
            Check(dlg, IDC_CHK_TRANSPARENT, ctx->edited.makeDraggedWindowTransparent);
            Check(dlg, IDC_CHK_AUTOSTART, Autostart::IsEnabled());
            Check(dlg, IDC_CHK_ZONE_NUMBER, ctx->edited.showZoneNumber);
            Check(dlg, IDC_CHK_ZONE_SIZE, ctx->edited.showZoneSize);

            SetDlgItemInt(dlg, IDC_EDIT_OPACITY, ctx->edited.highlightOpacity, FALSE);
            SetEditText(dlg, IDC_EDIT_ZONE_COLOR, ctx->edited.zoneColor);
            SetEditText(dlg, IDC_EDIT_BORDER_COLOR, ctx->edited.zoneBorderColor);
            SetEditText(dlg, IDC_EDIT_HIGHLIGHT_COLOR, ctx->edited.zoneHighlightColor);
            SetEditText(dlg, IDC_EDIT_NUMBER_COLOR, ctx->edited.zoneNumberColor);

            PopulateExcludedApps(dlg, ctx->edited.excludedApps);

            return TRUE;
        }

        case WM_COMMAND:
        {
            auto* ctx = reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));

            switch (LOWORD(wParam))
            {
            case IDC_BTN_ADD_APP:
            {
                std::wstring app = GetEditText(dlg, IDC_EDIT_EXCLUDED_APP);
                if (!app.empty())
                {
                    ctx->edited.excludedApps.push_back(std::move(app));
                    PopulateExcludedApps(dlg, ctx->edited.excludedApps);
                    SetEditText(dlg, IDC_EDIT_EXCLUDED_APP, L"");
                    SetFocus(GetDlgItem(dlg, IDC_EDIT_EXCLUDED_APP));
                }
                return TRUE;
            }
            case IDC_BTN_REMOVE_APP:
            {
                HWND list = GetDlgItem(dlg, IDC_LST_EXCLUDED_APPS);
                int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(ctx->edited.excludedApps.size()))
                {
                    ctx->edited.excludedApps.erase(ctx->edited.excludedApps.begin() + sel);
                    PopulateExcludedApps(dlg, ctx->edited.excludedApps);
                }
                return TRUE;
            }
            case IDOK:
            {
                ctx->edited.shiftDrag = IsChecked(dlg, IDC_CHK_SHIFT_DRAG);
                ctx->edited.mouseSwitch = IsChecked(dlg, IDC_CHK_MOUSE_SWITCH);
                ctx->edited.mouseMiddleClickSpanningMultipleZones = IsChecked(dlg, IDC_CHK_MOUSE_MIDDLE);
                ctx->edited.moveWindowAcrossMonitors = IsChecked(dlg, IDC_CHK_MOVE_ACROSS);
                ctx->edited.restoreSize = IsChecked(dlg, IDC_CHK_RESTORE_SIZE);
                ctx->edited.spanZonesAcrossMonitors = IsChecked(dlg, IDC_CHK_SPAN_MONITORS);
                ctx->edited.makeDraggedWindowTransparent = IsChecked(dlg, IDC_CHK_TRANSPARENT);
                Autostart::SetEnabled(IsChecked(dlg, IDC_CHK_AUTOSTART));
                ctx->edited.showZoneNumber = IsChecked(dlg, IDC_CHK_ZONE_NUMBER);
                ctx->edited.showZoneSize = IsChecked(dlg, IDC_CHK_ZONE_SIZE);

                int opacity = GetDlgItemInt(dlg, IDC_EDIT_OPACITY, nullptr, FALSE);
                ctx->edited.highlightOpacity = std::max(0, std::min(100, opacity));

                ctx->edited.zoneColor = GetEditText(dlg, IDC_EDIT_ZONE_COLOR);
                ctx->edited.zoneBorderColor = GetEditText(dlg, IDC_EDIT_BORDER_COLOR);
                ctx->edited.zoneHighlightColor = GetEditText(dlg, IDC_EDIT_HIGHLIGHT_COLOR);
                ctx->edited.zoneNumberColor = GetEditText(dlg, IDC_EDIT_NUMBER_COLOR);

                Settings::instance().data = ctx->edited;
                Settings::instance().Save();

                EndDialog(dlg, IDOK);
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
        }
        return FALSE;
    }
}

bool SettingsDialog::Show(HWND owner, HINSTANCE hInstance)
{
    DialogContext ctx{};
    ctx.edited = Settings::instance().data;

    const INT_PTR result = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_SETTINGS), owner,
                                           &SettingsDlgProc, reinterpret_cast<LPARAM>(&ctx));
    return result == IDOK;
}
