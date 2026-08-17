#include "json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    const Json kNullJson;

    const wchar_t* g_parsePos = nullptr;
    bool g_parseFailed = false;

    void SkipWhitespace()
    {
        while (*g_parsePos == L' ' || *g_parsePos == L'\t' || *g_parsePos == L'\r' || *g_parsePos == L'\n')
        {
            ++g_parsePos;
        }
    }

    bool ParseHex4(unsigned& value)
    {
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const wchar_t c = *g_parsePos;
            unsigned digit;
            if (c >= L'0' && c <= L'9')
            {
                digit = static_cast<unsigned>(c - L'0');
            }
            else if (c >= L'a' && c <= L'f')
            {
                digit = static_cast<unsigned>(c - L'a' + 10);
            }
            else if (c >= L'A' && c <= L'F')
            {
                digit = static_cast<unsigned>(c - L'A' + 10);
            }
            else
            {
                return false;
            }
            value = (value << 4) | digit;
            ++g_parsePos;
        }
        return true;
    }

    bool ParseString(std::wstring& out)
    {
        if (*g_parsePos != L'"')
        {
            return false;
        }
        ++g_parsePos;

        out.clear();
        while (true)
        {
            const wchar_t c = *g_parsePos;
            if (c == L'\0')
            {
                return false;
            }
            if (c == L'"')
            {
                ++g_parsePos;
                return true;
            }
            if (c == L'\\')
            {
                ++g_parsePos;
                const wchar_t esc = *g_parsePos;
                switch (esc)
                {
                case L'"':
                case L'\\':
                case L'/':
                    out.push_back(esc);
                    break;
                case L'b':
                    out.push_back(L'\b');
                    break;
                case L'f':
                    out.push_back(L'\f');
                    break;
                case L'n':
                    out.push_back(L'\n');
                    break;
                case L'r':
                    out.push_back(L'\r');
                    break;
                case L't':
                    out.push_back(L'\t');
                    break;
                case L'u':
                {
                    ++g_parsePos;
                    unsigned code = 0;
                    if (!ParseHex4(code))
                    {
                        return false;
                    }
                    if (code >= 0xD800 && code <= 0xDBFF)
                    {
                        // High surrogate: expect a low surrogate right after.
                        if (g_parsePos[0] == L'\\' && g_parsePos[1] == L'u')
                        {
                            g_parsePos += 2;
                            unsigned low = 0;
                            if (!ParseHex4(low) || low < 0xDC00 || low > 0xDFFF)
                            {
                                return false;
                            }
                            out.push_back(static_cast<wchar_t>(code));
                            out.push_back(static_cast<wchar_t>(low));
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        out.push_back(static_cast<wchar_t>(code));
                    }
                    break;
                }
                default:
                    return false;
                }
                ++g_parsePos;
            }
            else
            {
                out.push_back(c);
                ++g_parsePos;
            }
        }
    }

    Json ParseValue();

    bool ParseArray(Json& out)
    {
        // g_parsePos at '['
        ++g_parsePos;
        SkipWhitespace();
        if (*g_parsePos == L']')
        {
            ++g_parsePos;
            return true;
        }
        while (true)
        {
            const Json item = ParseValue();
            if (g_parseFailed)
            {
                return false;
            }
            out.Push(item);
            SkipWhitespace();
            if (*g_parsePos == L',')
            {
                ++g_parsePos;
                SkipWhitespace();
            }
            else if (*g_parsePos == L']')
            {
                ++g_parsePos;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    bool ParseObject(Json& out)
    {
        // g_parsePos at '{'
        ++g_parsePos;
        SkipWhitespace();
        if (*g_parsePos == L'}')
        {
            ++g_parsePos;
            return true;
        }
        while (true)
        {
            SkipWhitespace();
            std::wstring key;
            if (!ParseString(key))
            {
                return false;
            }
            SkipWhitespace();
            if (*g_parsePos != L':')
            {
                return false;
            }
            ++g_parsePos;
            SkipWhitespace();
            const Json value = ParseValue();
            if (g_parseFailed)
            {
                return false;
            }
            out.Set(key, value);
            SkipWhitespace();
            if (*g_parsePos == L',')
            {
                ++g_parsePos;
                SkipWhitespace();
            }
            else if (*g_parsePos == L'}')
            {
                ++g_parsePos;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    Json ParseValue()
    {
        SkipWhitespace();
        if (g_parseFailed)
        {
            return Json::MakeNull();
        }

        const wchar_t c = *g_parsePos;
        if (c == L'{')
        {
            Json obj = Json::MakeObject();
            if (!ParseObject(obj))
            {
                g_parseFailed = true;
            }
            return obj;
        }
        if (c == L'[')
        {
            Json arr = Json::MakeArray();
            if (!ParseArray(arr))
            {
                g_parseFailed = true;
            }
            return arr;
        }
        if (c == L'"')
        {
            std::wstring value;
            if (!ParseString(value))
            {
                g_parseFailed = true;
                return Json::MakeNull();
            }
            return Json::MakeString(value);
        }
        if (c == L't')
        {
            if (g_parsePos[1] == L'r' && g_parsePos[2] == L'u' && g_parsePos[3] == L'e')
            {
                g_parsePos += 4;
                return Json::MakeBool(true);
            }
            g_parseFailed = true;
            return Json::MakeNull();
        }
        if (c == L'f')
        {
            if (g_parsePos[1] == L'a' && g_parsePos[2] == L'l' && g_parsePos[3] == L's' && g_parsePos[4] == L'e')
            {
                g_parsePos += 5;
                return Json::MakeBool(false);
            }
            g_parseFailed = true;
            return Json::MakeNull();
        }
        if (c == L'n')
        {
            if (g_parsePos[1] == L'u' && g_parsePos[2] == L'l' && g_parsePos[3] == L'l')
            {
                g_parsePos += 4;
                return Json::MakeNull();
            }
            g_parseFailed = true;
            return Json::MakeNull();
        }
        if (c == L'-' || (c >= L'0' && c <= L'9'))
        {
            const wchar_t* start = g_parsePos;
            if (c == L'-')
            {
                ++g_parsePos;
            }
            while (*g_parsePos >= L'0' && *g_parsePos <= L'9')
            {
                ++g_parsePos;
            }
            if (*g_parsePos == L'.')
            {
                ++g_parsePos;
                while (*g_parsePos >= L'0' && *g_parsePos <= L'9')
                {
                    ++g_parsePos;
                }
            }
            if (*g_parsePos == L'e' || *g_parsePos == L'E')
            {
                ++g_parsePos;
                if (*g_parsePos == L'+' || *g_parsePos == L'-')
                {
                    ++g_parsePos;
                }
                while (*g_parsePos >= L'0' && *g_parsePos <= L'9')
                {
                    ++g_parsePos;
                }
            }

            std::wstring token(start, g_parsePos);
            wchar_t* end = nullptr;
            const double value = std::wcstod(token.c_str(), &end);
            if (end == token.c_str())
            {
                g_parseFailed = true;
                return Json::MakeNull();
            }
            return Json::MakeNumber(value);
        }

        g_parseFailed = true;
        return Json::MakeNull();
    }

    void EscapeString(const std::wstring& value, std::wstring& out)
    {
        out.push_back(L'"');
        for (const wchar_t c : value)
        {
            switch (c)
            {
            case L'"':
                out += L"\\\"";
                break;
            case L'\\':
                out += L"\\\\";
                break;
            case L'\b':
                out += L"\\b";
                break;
            case L'\f':
                out += L"\\f";
                break;
            case L'\n':
                out += L"\\n";
                break;
            case L'\r':
                out += L"\\r";
                break;
            case L'\t':
                out += L"\\t";
                break;
            default:
                if (static_cast<unsigned>(c) < 0x20)
                {
                    wchar_t buf[8];
                    swprintf_s(buf, L"\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                }
                else
                {
                    out.push_back(c);
                }
                break;
            }
        }
        out.push_back(L'"');
    }

    void AppendIndent(std::wstring& out, int level)
    {
        out.append(static_cast<size_t>(level) * 2, L' ');
    }
}

Json::Json() = default;

Json Json::MakeNull()
{
    return Json();
}

Json Json::MakeBool(bool value)
{
    Json j;
    j.m_type = Type::Bool;
    j.m_bool = value;
    return j;
}

Json Json::MakeNumber(double value)
{
    Json j;
    j.m_type = Type::Number;
    j.m_number = value;
    return j;
}

Json Json::MakeString(const std::wstring& value)
{
    Json j;
    j.m_type = Type::String;
    j.m_string = value;
    return j;
}

Json Json::MakeArray()
{
    Json j;
    j.m_type = Type::Array;
    return j;
}

Json Json::MakeObject()
{
    Json j;
    j.m_type = Type::Object;
    return j;
}

Json::Type Json::type() const
{
    return m_type;
}

bool Json::isNull() const
{
    return m_type == Type::Null;
}

bool Json::Has(const std::wstring& key) const
{
    return Find(key) != nullptr;
}

const Json* Json::Find(const std::wstring& key) const
{
    if (m_type != Type::Object)
    {
        return nullptr;
    }
    const auto it = std::find_if(m_object.begin(), m_object.end(), [&key](const auto& entry) { return entry.first == key; });
    return it == m_object.end() ? nullptr : &it->second;
}

const Json& Json::At(const std::wstring& key) const
{
    const Json* found = Find(key);
    return found ? *found : kNullJson;
}

void Json::Set(const std::wstring& key, const Json& value)
{
    if (m_type != Type::Object)
    {
        m_type = Type::Object;
        m_object.clear();
    }
    const auto it = std::find_if(m_object.begin(), m_object.end(), [&key](const auto& entry) { return entry.first == key; });
    if (it == m_object.end())
    {
        m_object.emplace_back(key, value);
    }
    else
    {
        it->second = value;
    }
}

void Json::Set(const std::wstring& key, bool value)
{
    Set(key, MakeBool(value));
}

void Json::Set(const std::wstring& key, double value)
{
    Set(key, MakeNumber(value));
}

void Json::Set(const std::wstring& key, const std::wstring& value)
{
    Set(key, MakeString(value));
}

void Json::Push(const Json& value)
{
    if (m_type != Type::Array)
    {
        m_type = Type::Array;
        m_array.clear();
    }
    m_array.push_back(value);
}

size_t Json::Size() const
{
    if (m_type == Type::Array)
    {
        return m_array.size();
    }
    if (m_type == Type::Object)
    {
        return m_object.size();
    }
    return 0;
}

const Json& Json::At(size_t index) const
{
    if (m_type == Type::Array && index < m_array.size())
    {
        return m_array[index];
    }
    return kNullJson;
}

double Json::AsNumber(double fallback) const
{
    return m_type == Type::Number ? m_number : fallback;
}

bool Json::AsBool(bool fallback) const
{
    return m_type == Type::Bool ? m_bool : fallback;
}

std::wstring Json::AsString(const std::wstring& fallback) const
{
    return m_type == Type::String ? m_string : fallback;
}

void Json::AppendSerialized(std::wstring& out, int indentLevel, bool compact) const
{
    switch (m_type)
    {
    case Type::Null:
        out += L"null";
        break;
    case Type::Bool:
        out += m_bool ? L"true" : L"false";
        break;
    case Type::Number:
    {
        if (std::floor(m_number) == m_number && std::abs(m_number) < 1e15)
        {
            wchar_t buf[32];
            swprintf_s(buf, L"%lld", static_cast<long long>(m_number));
            out += buf;
        }
        else
        {
            wchar_t buf[32];
            swprintf_s(buf, L"%.9g", m_number);
            out += buf;
        }
        break;
    }
    case Type::String:
        EscapeString(m_string, out);
        break;
    case Type::Array:
    {
        if (m_array.empty())
        {
            out += L"[]";
            break;
        }
        out += L'[';
        for (size_t i = 0; i < m_array.size(); ++i)
        {
            if (i > 0)
            {
                out += L',';
            }
            if (!compact)
            {
                out += L'\n';
                AppendIndent(out, indentLevel + 1);
            }
            m_array[i].AppendSerialized(out, indentLevel + 1, compact);
        }
        if (!compact)
        {
            out += L'\n';
            AppendIndent(out, indentLevel);
        }
        out += L']';
        break;
    }
    case Type::Object:
    {
        if (m_object.empty())
        {
            out += L"{}";
            break;
        }
        out += L'{';
        for (size_t i = 0; i < m_object.size(); ++i)
        {
            if (i > 0)
            {
                out += L',';
            }
            if (!compact)
            {
                out += L'\n';
                AppendIndent(out, indentLevel + 1);
            }
            EscapeString(m_object[i].first, out);
            out += L':';
            if (!compact)
            {
                out += L' ';
            }
            m_object[i].second.AppendSerialized(out, indentLevel + 1, compact);
        }
        if (!compact)
        {
            out += L'\n';
            AppendIndent(out, indentLevel);
        }
        out += L'}';
        break;
    }
    }
}

std::wstring Json::SerializeIndented() const
{
    std::wstring out;
    AppendSerialized(out, 0, false);
    return out;
}

std::wstring Json::SerializeCompact() const
{
    std::wstring out;
    AppendSerialized(out, 0, true);
    return out;
}

bool Json::Parse(const std::wstring& text, Json& out)
{
    g_parsePos = text.c_str();
    g_parseFailed = false;

    const Json value = ParseValue();
    SkipWhitespace();
    if (g_parseFailed || *g_parsePos != L'\0')
    {
        return false;
    }

    out = value;
    return true;
}
