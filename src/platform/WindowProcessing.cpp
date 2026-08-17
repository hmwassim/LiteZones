#include "WindowProcessing.h"

#include "Settings.h"
#include "WindowUtils.h"

#include <cwctype>
#include <string>

namespace
{
    constexpr int kClassNameBufferSize = 256;

    std::wstring Lowercase(std::wstring text)
    {
        for (wchar_t& c : text)
        {
            c = static_cast<wchar_t>(std::towlower(c));
        }
        return text;
    }

    bool MatchesExclusion(HWND window, const std::wstring& processPath, const std::wstring& excluded)
    {
        if (excluded.empty())
        {
            return false;
        }

        const std::wstring lowerNeedle = Lowercase(excluded);

        if (Lowercase(processPath).find(lowerNeedle) != std::wstring::npos)
        {
            return true;
        }

        wchar_t className[kClassNameBufferSize]{};
        GetClassNameW(window, className, kClassNameBufferSize);
        return Lowercase(className).find(lowerNeedle) != std::wstring::npos;
    }

    bool IsExcludedByUser(HWND window, const std::wstring& processPath, const SettingsData& settings)
    {
        if (settings.excludedApps.empty())
        {
            return false;
        }

        for (const auto& excluded : settings.excludedApps)
        {
            if (MatchesExclusion(window, processPath, excluded))
            {
                return true;
            }
        }

        return false;
    }

    bool IsExcludedByDefault(HWND window, const std::wstring& processPath)
    {
        wchar_t selfPath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, selfPath, MAX_PATH) > 0)
        {
            if (Lowercase(selfPath) == Lowercase(processPath))
            {
                return true;
            }
        }

        wchar_t className[kClassNameBufferSize]{};
        GetClassNameW(window, className, kClassNameBufferSize);
        const std::wstring lowerClass = Lowercase(className);
        if (lowerClass.find(L"shell_") != std::wstring::npos)
        {
            return true;
        }
        if (lowerClass == L"progman")
        {
            return true;
        }

        return false;
    }
}

namespace WindowProcessing
{
    bool IsProcessableManually(HWND window, const SettingsData& settings) noexcept
    {
        if (!IsWindow(window))
        {
            return false;
        }

        if (IsIconic(window))
        {
            return false;
        }

        const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
        const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);

        if (!(style & WS_VISIBLE))
        {
            return false;
        }

        if (exStyle & WS_EX_TOOLWINDOW)
        {
            return false;
        }

        if (!WindowUtils::IsRoot(window))
        {
            return false;
        }

        const bool isPopup = (style & WS_POPUP) != 0;
        const bool hasThickFrame = (style & WS_THICKFRAME) != 0;
        const bool hasCaption = (style & WS_CAPTION) != 0;
        const bool hasMinimizeMaximizeButtons = (style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) != 0;
        if (isPopup && !(hasThickFrame && (hasCaption || hasMinimizeMaximizeButtons)))
        {
            return false;
        }

        if (WindowUtils::HasVisibleOwner(window))
        {
            return false;
        }

        const std::wstring processPath = WindowUtils::GetProcessPath(window);
        if (IsExcludedByUser(window, processPath, settings))
        {
            return false;
        }
        if (IsExcludedByDefault(window, processPath))
        {
            return false;
        }

        return true;
    }
}
