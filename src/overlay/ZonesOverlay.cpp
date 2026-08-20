#include "ZonesOverlay.h"

#include "MonitorManager.h"

#include <d2d1helper.h>
#include <dxgiformat.h>

#include <string>

namespace
{
    D2D1_COLOR_F ToColorF(COLORREF color, float alpha) noexcept
    {
        D2D1_COLOR_F result;
        result.r = static_cast<float>(GetRValue(color)) / 255.f;
        result.g = static_cast<float>(GetGValue(color)) / 255.f;
        result.b = static_cast<float>(GetBValue(color)) / 255.f;
        result.a = alpha;
        return result;
    }

    constexpr float kDefaultDpi = 96.f;

    D2D1_RECT_F ToRectF(const RECT& rect, float dpiScale) noexcept
    {
        const float inv = 1.f / dpiScale;
        D2D1_RECT_F result;
        result.left = static_cast<float>(rect.left) * inv + 0.5f;
        result.top = static_cast<float>(rect.top) * inv + 0.5f;
        result.right = static_cast<float>(rect.right) * inv - 0.5f;
        result.bottom = static_cast<float>(rect.bottom) * inv - 0.5f;
        return result;
    }
}

ZonesOverlay::ZonesOverlay(HWND window) :
    m_window(window)
{
}

ZonesOverlay::~ZonesOverlay()
{
    ReleaseDeviceResources();
    if (m_textFormat)
    {
        m_textFormat->Release();
    }
    if (m_sizeFormat)
    {
        m_sizeFormat->Release();
    }
    if (m_writeFactory)
    {
        m_writeFactory->Release();
    }
    if (m_d2dFactory)
    {
        m_d2dFactory->Release();
    }
}

void ZonesOverlay::ReleaseDeviceResources()
{
    // Brushes are device-dependent (created from m_renderTarget), so they
    // must be dropped and recreated whenever the render target is. Text
    // formats and m_writeFactory are device-independent and outlive this.
    if (m_fillBrush)
    {
        m_fillBrush->Release();
        m_fillBrush = nullptr;
    }
    if (m_borderBrush)
    {
        m_borderBrush->Release();
        m_borderBrush = nullptr;
    }
    if (m_textBrush)
    {
        m_textBrush->Release();
        m_textBrush = nullptr;
    }
    if (m_renderTarget)
    {
        m_renderTarget->Release();
        m_renderTarget = nullptr;
    }
}

bool ZonesOverlay::EnsureResources()
{
    if (m_renderTarget)
    {
        return true;
    }

    RECT clientRect{};
    if (!GetClientRect(m_window, &clientRect))
    {
        return false;
    }

    const D2D1_SIZE_U size{
        static_cast<UINT32>(clientRect.right - clientRect.left),
        static_cast<UINT32>(clientRect.bottom - clientRect.top)
    };
    if (size.width == 0 || size.height == 0)
    {
        return false;
    }

    if (!m_d2dFactory)
    {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory)))
        {
            return false;
        }
    }

    // Use the monitor's actual DPI for the D2D render target so zone
    // coordinates (physical pixels) and text are scaled correctly.
    const UINT dpi = MonitorUtils::GetDpiForMonitor(MonitorFromWindow(m_window, MONITOR_DEFAULTTONEAREST));
    const float dpiF = static_cast<float>(dpi);
    m_dpiScale = dpiF / kDefaultDpi;

    D2D1_RENDER_TARGET_PROPERTIES rtProps{};
    rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtProps.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
    rtProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    rtProps.dpiX = dpiF;
    rtProps.dpiY = dpiF;

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
    hwndProps.hwnd = m_window;
    hwndProps.pixelSize = size;
    hwndProps.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    if (FAILED(m_d2dFactory->CreateHwndRenderTarget(rtProps, hwndProps, &m_renderTarget)))
    {
        m_renderTarget = nullptr;
        return false;
    }

    // Brushes are device-dependent: created once per render target and
    // reused every frame via SetColor() instead of being
    // created/destroyed per rectangle in Render().
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), &m_fillBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), &m_borderBrush);
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), &m_textBrush);
    if (!m_fillBrush || !m_borderBrush || !m_textBrush)
    {
        ReleaseDeviceResources();
        return false;
    }

    if (!m_writeFactory && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_writeFactory))))
    {
        ReleaseDeviceResources();
        return false;
    }

    // Text formats are device-independent (owned by m_writeFactory, not the
    // render target), so they're created once for the life of the overlay
    // rather than every Render() call.
    if (!m_textFormat && SUCCEEDED(m_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 80.f, L"en-US", &m_textFormat)))
    {
        m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!m_sizeFormat && SUCCEEDED(m_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.f, L"en-US", &m_sizeFormat)))
    {
        m_sizeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_sizeFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return true;
}

void ZonesOverlay::DrawActiveZoneSet(const ZonesMap& zones, const ZoneIndexSet& highlightZones, const Colors::ZoneColors& colors, bool showZoneText, bool showZoneSize)
{
    m_rects.clear();
    m_colors = colors;
    m_showZoneText = showZoneText;
    m_showZoneSize = showZoneSize;

    std::vector<bool> isHighlighted(static_cast<size_t>(zones.size() + 1), false);
    for (ZoneIndex index : highlightZones)
    {
        if (index >= 0 && static_cast<size_t>(index) < isHighlighted.size())
        {
            isHighlighted[static_cast<size_t>(index)] = true;
        }
    }

    // Draw the inactive zones first, then the highlighted ones on top.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto& [id, zone] : zones)
        {
            const bool highlighted = isHighlighted[static_cast<size_t>(id)];
            if (highlighted != (pass == 1))
            {
                continue;
            }

            DrawableRect item;
            item.rect = ToRectF(zone.GetZoneRect(), m_dpiScale);
            item.id = static_cast<int>(id);
            item.highlighted = highlighted;
            const RECT zr = zone.GetZoneRect();
            item.pixelWidth = zr.right - zr.left;
            item.pixelHeight = zr.bottom - zr.top;
            m_rects.push_back(item);
        }
    }
}

void ZonesOverlay::Show()
{
    ShowWindow(m_window, SW_SHOWNA);
    Render();
}

void ZonesOverlay::Hide()
{
    ShowWindow(m_window, SW_HIDE);
}

bool ZonesOverlay::PreWarm()
{
    return EnsureResources();
}

void ZonesOverlay::Render()
{
    if (!EnsureResources())
    {
        return;
    }

    const float opacity = static_cast<float>(m_colors.highlightOpacity) / 100.f;
    const D2D1_COLOR_F borderColor = ToColorF(m_colors.borderColor, 1.f);
    const D2D1_COLOR_F inactiveColor = ToColorF(m_colors.primaryColor, opacity);
    const D2D1_COLOR_F highlightColor = ToColorF(m_colors.highlightColor, opacity);
    const D2D1_COLOR_F textColor = ToColorF(m_colors.numberColor, 1.f);

    const bool drawText = m_showZoneText && m_textFormat;
    const bool drawSize = drawText && m_showZoneSize && m_sizeFormat;

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));

    // Border and text colors are constant for the whole frame; only the
    // fill color varies (highlighted vs. inactive), so set those two once
    // instead of per rectangle.
    m_borderBrush->SetColor(borderColor);
    if (drawText)
    {
        m_textBrush->SetColor(textColor);
    }

    for (const auto& item : m_rects)
    {
        const D2D1_COLOR_F& fill = item.highlighted ? highlightColor : inactiveColor;
        m_fillBrush->SetColor(fill);

        m_renderTarget->FillRectangle(item.rect, m_fillBrush);
        m_renderTarget->DrawRectangle(item.rect, m_borderBrush);

        if (drawText)
        {
            const std::wstring idStr = std::to_wstring(item.id + 1);
            if (drawSize)
            {
                D2D1_RECT_F topHalf = item.rect;
                topHalf.bottom = (item.rect.top + item.rect.bottom) / 2.f;
                m_renderTarget->DrawTextW(idStr.c_str(), static_cast<UINT32>(idStr.size()), m_textFormat, topHalf, m_textBrush);

                const int w = item.pixelWidth;
                const int h = item.pixelHeight;
                wchar_t sizeBuf[32]{};
                swprintf_s(sizeBuf, L"%dx%d", w, h);
                D2D1_RECT_F bottomHalf = item.rect;
                bottomHalf.top = (item.rect.top + item.rect.bottom) / 2.f;
                m_renderTarget->DrawTextW(sizeBuf, static_cast<UINT32>(wcslen(sizeBuf)), m_sizeFormat, bottomHalf, m_textBrush);
            }
            else
            {
                m_renderTarget->DrawTextW(idStr.c_str(), static_cast<UINT32>(idStr.size()), m_textFormat, item.rect, m_textBrush);
            }
        }
    }

    const HRESULT endHr = m_renderTarget->EndDraw();
    if (endHr == D2DERR_RECREATE_TARGET)
    {
        ReleaseDeviceResources();
    }
}
