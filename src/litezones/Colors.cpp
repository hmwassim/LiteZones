#include "Colors.h"

#include "Settings.h"

namespace
{
    int HexDigit(wchar_t c)
    {
        if (c >= L'0' && c <= L'9')
        {
            return c - L'0';
        }
        if (c >= L'a' && c <= L'f')
        {
            return c - L'a' + 10;
        }
        if (c >= L'A' && c <= L'F')
        {
            return c - L'A' + 10;
        }
        return -1;
    }

    void HexToRgb(const std::wstring& text, COLORREF& out)
    {
        if (text.size() != 7 || text[0] != L'#')
        {
            return;
        }

        int value = 0;
        for (size_t i = 1; i < text.size(); ++i)
        {
            const int digit = HexDigit(text[i]);
            if (digit < 0)
            {
                return;
            }
            value = value * 16 + digit;
        }

        out = RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }
}

namespace Colors
{
    ZoneColors GetZoneColors(const SettingsData& settings) noexcept
    {
        ZoneColors colors{};
        colors.highlightOpacity = settings.highlightOpacity;

        HexToRgb(settings.zoneColor, colors.primaryColor);
        HexToRgb(settings.zoneBorderColor, colors.borderColor);
        HexToRgb(settings.zoneHighlightColor, colors.highlightColor);
        HexToRgb(settings.zoneNumberColor, colors.numberColor);

        return colors;
    }
}
