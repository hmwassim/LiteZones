#include "GuidUtils.h"

namespace
{
    int HexValue(wchar_t c)
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
}

namespace Util
{
    bool GuidFromString(const std::wstring& str, GUID& out) noexcept
    {
        if (str.size() != 36)
        {
            return false;
        }

        unsigned char bytes[16]{};
        size_t hexCount = 0;
        for (size_t i = 0; i < str.size(); ++i)
        {
            const wchar_t c = str[i];
            if (c == L'-')
            {
                continue;
            }
            const int value = HexValue(c);
            if (value < 0)
            {
                return false;
            }
            if (hexCount % 2 == 0)
            {
                bytes[hexCount / 2] = static_cast<unsigned char>(value << 4);
            }
            else
            {
                bytes[hexCount / 2] = static_cast<unsigned char>(bytes[hexCount / 2] | value);
            }
            ++hexCount;
        }
        if (hexCount != 32)
        {
            return false;
        }

        out.Data1 = (static_cast<unsigned long>(bytes[0]) << 24) |
                    (static_cast<unsigned long>(bytes[1]) << 16) |
                    (static_cast<unsigned long>(bytes[2]) << 8) |
                    static_cast<unsigned long>(bytes[3]);
        out.Data2 = static_cast<unsigned short>((bytes[4] << 8) | bytes[5]);
        out.Data3 = static_cast<unsigned short>((bytes[6] << 8) | bytes[7]);
        for (size_t i = 0; i < 8; ++i)
        {
            out.Data4[i] = bytes[8 + i];
        }
        return true;
    }

    std::wstring GuidToString(const GUID& guid) noexcept
    {
        wchar_t buffer[64]{};
        wsprintfW(buffer, L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                  guid.Data1, guid.Data2, guid.Data3,
                  guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                  guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return buffer;
    }

    std::wstring TypeToString(LiteZonesTypes::ZoneSetLayoutType type) noexcept
    {
        switch (type)
        {
        case LiteZonesTypes::ZoneSetLayoutType::Rows:
            return L"rows";
        case LiteZonesTypes::ZoneSetLayoutType::Columns:
            return L"columns";
        case LiteZonesTypes::ZoneSetLayoutType::Grid:
            return L"grid";
        case LiteZonesTypes::ZoneSetLayoutType::PriorityGrid:
            return L"priority-grid";
        case LiteZonesTypes::ZoneSetLayoutType::Custom:
            return L"custom";
        }
        return L"rows";
    }

    LiteZonesTypes::ZoneSetLayoutType TypeFromString(const std::wstring& value) noexcept
    {
        if (value == L"rows")
        {
            return LiteZonesTypes::ZoneSetLayoutType::Rows;
        }
        if (value == L"columns")
        {
            return LiteZonesTypes::ZoneSetLayoutType::Columns;
        }
        if (value == L"grid")
        {
            return LiteZonesTypes::ZoneSetLayoutType::Grid;
        }
        if (value == L"priority-grid")
        {
            return LiteZonesTypes::ZoneSetLayoutType::PriorityGrid;
        }
        if (value == L"custom")
        {
            return LiteZonesTypes::ZoneSetLayoutType::Custom;
        }
        return LiteZonesTypes::ZoneSetLayoutType::Rows;
    }
}
