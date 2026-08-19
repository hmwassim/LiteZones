#include "EditorCanvasInternal.h"

#include "Colors.h"
#include "LayoutEngine.h"

#include <algorithm>

namespace EditorCanvasInternal
{
    void DrawTextCenter(HDC dc, const RECT& rect, int value, COLORREF color)
    {
        wchar_t number[16]{};
        wsprintfW(number, L"%d", value);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, number, -1, const_cast<RECT*>(&rect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void DrawZoneLabel(HDC dc, const RECT& screenRect, int index, int pixelW, int pixelH, COLORREF color)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);

        wchar_t dim[32]{};
        swprintf_s(dim, L"%d\u00D7%d", pixelW, pixelH);

        if (index < 0)
        {
            DrawTextW(dc, dim, -1, const_cast<RECT*>(&screenRect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        const int midY = (screenRect.top + screenRect.bottom) / 2;

        wchar_t num[16]{};
        wsprintfW(num, L"%d", index);
        RECT topHalf{ screenRect.left, screenRect.top, screenRect.right, midY };
        DrawTextW(dc, num, -1, &topHalf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT botHalf{ screenRect.left, midY, screenRect.right, screenRect.bottom };
        DrawTextW(dc, dim, -1, &botHalf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void DrawView(HWND hwnd, HDC dc)
    {
        const CanvasView& view = View();
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

        const float scale = ComputeScale(view, width, height);
        const int offsetX = static_cast<int>((static_cast<float>(width) - static_cast<float>(view.virtualWidth) * scale) / 2.0f);
        const int offsetY = static_cast<int>((static_cast<float>(height) - static_cast<float>(view.virtualHeight) * scale) / 2.0f);

        const Colors::ZoneColors colors = view.settings ? Colors::GetZoneColors(*view.settings) : Colors::ZoneColors{};
        HBRUSH fillBrush = CreateSolidBrush(colors.primaryColor);
        HBRUSH borderBrush = CreateSolidBrush(colors.borderColor);
        HBRUSH highlightBrush = CreateSolidBrush(colors.highlightColor);
        HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
        HPEN borderPen = CreatePen(PS_SOLID, 1, colors.borderColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, borderPen));

        std::vector<EditorCanvas::ZoneRect> drawnZones;
        if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
        {
            // Convert each cell from Multiplier space to virtual pixels, then
            // apply the same spacing inset the runtime layout engine applies
            // (LayoutEngine's ApplyGridCellSpacing / CalculateGridZones), so
            // the preview's drawn size - and its WxH label - match what
            // dragging a window onto this layout will actually show.
            for (const auto& zone : view.grid->Zones())
            {
                const RECT rawRect{
                    zone.left * view.virtualWidth / GridData::Multiplier,
                    zone.top * view.virtualHeight / GridData::Multiplier,
                    zone.right * view.virtualWidth / GridData::Multiplier,
                    zone.bottom * view.virtualHeight / GridData::Multiplier
                };
                const RECT spacedRect = ApplyGridCellSpacing(
                    rawRect,
                    zone.top == 0, zone.bottom == GridData::Multiplier,
                    zone.left == 0, zone.right == GridData::Multiplier,
                    view.spacingPixels);
                drawnZones.push_back(EditorCanvas::ZoneRect{ spacedRect, zone.index });
            }
        }
        else if (view.mode == EditorCanvas::Mode::CanvasEdit)
        {
            drawnZones = CanvasModelZones(view);
        }
        else
        {
            drawnZones = view.zones;
        }

        for (const auto& zone : drawnZones)
        {
            RECT rect{
                offsetX + static_cast<int>(static_cast<float>(zone.rect.left) * scale),
                offsetY + static_cast<int>(static_cast<float>(zone.rect.top) * scale),
                offsetX + static_cast<int>(static_cast<float>(zone.rect.right) * scale),
                offsetY + static_cast<int>(static_cast<float>(zone.rect.bottom) * scale)
            };

            FillRect(dc, &rect, fillBrush);

            const bool selected = std::find(view.selectedZones.begin(), view.selectedZones.end(), zone.index) != view.selectedZones.end();
            if (selected)
            {
                HDC memDC = CreateCompatibleDC(dc);
                HBITMAP memBmp = CreateCompatibleBitmap(dc, rect.right - rect.left, rect.bottom - rect.top);
                HGDIOBJ oldBmp = SelectObject(memDC, memBmp);
                RECT fill{ 0, 0, rect.right - rect.left, rect.bottom - rect.top };
                FillRect(memDC, &fill, highlightBrush);
                BLENDFUNCTION bf{};
                bf.BlendOp = AC_SRC_OVER;
                bf.SourceConstantAlpha = colors.highlightOpacity > 0
                                             ? static_cast<BYTE>(std::min(colors.highlightOpacity, 255))
                                             : 60;
                bf.AlphaFormat = 0;
                AlphaBlend(dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                           memDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, bf);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            HBRUSH frameBrush = selected ? highlightBrush : borderBrush;
            FrameRect(dc, &rect, frameBrush);

            const int pixW = zone.rect.right - zone.rect.left;
            const int pixH = zone.rect.bottom - zone.rect.top;
            DrawZoneLabel(dc, rect, zone.index + 1, pixW, pixH, colors.numberColor);
        }

        if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel &&
            view.selectedCanvasZone >= 0 && view.selectedCanvasZone < static_cast<int>(view.canvasModel->zones.size()))
        {
            const auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
            const RECT zoneRect{ zone.x, zone.y, zone.x + zone.width, zone.y + zone.height };
            for (int handle = CanvasMath::NW; handle <= CanvasMath::W; ++handle)
            {
                const RECT hr = CanvasHandleRect(zoneRect, handle);
                RECT drawHandle{
                    offsetX + static_cast<int>(static_cast<float>(hr.left) * scale),
                    offsetY + static_cast<int>(static_cast<float>(hr.top) * scale),
                    offsetX + static_cast<int>(static_cast<float>(hr.right) * scale),
                    offsetY + static_cast<int>(static_cast<float>(hr.bottom) * scale)
                };
                FillRect(dc, &drawHandle, highlightBrush);
                FrameRect(dc, &drawHandle, borderBrush);
            }
        }

        if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasDrawing)
        {
            const RECT rr = CanvasMath::Normalize(view.canvasDrawRect);
            RECT drawRect{
                offsetX + static_cast<int>(static_cast<float>(rr.left) * scale),
                offsetY + static_cast<int>(static_cast<float>(rr.top) * scale),
                offsetX + static_cast<int>(static_cast<float>(rr.right) * scale),
                offsetY + static_cast<int>(static_cast<float>(rr.bottom) * scale)
            };
            FrameRect(dc, &drawRect, highlightBrush);
            const int pixW = rr.right - rr.left;
            const int pixH = rr.bottom - rr.top;
            if (pixW > 0 && pixH > 0)
            {
                DrawZoneLabel(dc, drawRect, -1, pixW, pixH, colors.numberColor);
            }
        }

        if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
        {
            SelectObject(dc, borderPen);
            const auto boundaries = view.grid->BoundarySegments();
            for (const auto& segment : boundaries)
            {
                POINT start{};
                start.x = offsetX + static_cast<int>(static_cast<float>(segment.left * view.virtualWidth / GridData::Multiplier) * scale);
                start.y = offsetY + static_cast<int>(static_cast<float>(segment.top * view.virtualHeight / GridData::Multiplier) * scale);
                POINT end{};
                end.x = offsetX + static_cast<int>(static_cast<float>(segment.right * view.virtualWidth / GridData::Multiplier) * scale);
                end.y = offsetY + static_cast<int>(static_cast<float>(segment.bottom * view.virtualHeight / GridData::Multiplier) * scale);
                MoveToEx(dc, start.x, start.y, nullptr);
                LineTo(dc, end.x, end.y);
            }
        }

        if (drawnZones.empty())
        {
            RECT area{ 0, 0, width, height };
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(120, 120, 120));
            DrawTextW(dc, L"(no zones)", -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(dc, oldPen);
        SelectObject(dc, oldFont);
        DeleteObject(borderPen);
        DeleteObject(font);
        DeleteObject(fillBrush);
        DeleteObject(borderBrush);
        DeleteObject(highlightBrush);
    }
}
