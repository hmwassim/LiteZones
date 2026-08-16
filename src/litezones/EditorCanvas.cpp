#include "EditorCanvas.h"

#include "CanvasMath.h"
#include "Colors.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

// Forward-declare so the anonymous namespace's CanvasProc can call it.
namespace EditorCanvas
{
    bool CancelActiveOperation(HWND hwnd);
}

namespace
{
    constexpr wchar_t kCanvasClassName[] = L"LiteZonesEditorCanvas";

    // Distance (in client pixels) that picks a resizer for dragging / cursor feedback.
    constexpr int kResizerHitThreshold = 8;

    enum class CanvasInteraction
    {
        None,
        Draw,
        Move,
        Resize
    };

    struct CanvasView
    {
        EditorCanvas::Mode mode = EditorCanvas::Mode::Preview;
        int virtualWidth = 1600;
        int virtualHeight = 900;
        int clientWidth = 0;
        int clientHeight = 0;
        std::vector<EditorCanvas::ZoneRect> zones;
        std::optional<GridData::Grid> grid;
        std::vector<int> selectedZones;
        int dragResizer = -1;
        int dragLastMultiplier = 0;

        FancyZonesDataTypes::CanvasLayoutInfo* canvasModel = nullptr;
        int selectedCanvasZone = -1;
        CanvasInteraction canvasInteraction = CanvasInteraction::None;
        int canvasResizeHandle = CanvasMath::None;
        RECT canvasDragOrigin{};
        POINT canvasDragAnchor{};
        RECT canvasDrawRect{};
        bool canvasDrawing = false;

        // Pre-drag snapshot for grid resizer cancel-revert.
        FancyZonesDataTypes::GridLayoutInfo gridSnapshot{};

        // Callback fired on every committed edit (drag-end, split, merge, delete).
        std::function<void()> onEdited;

        // Callback fired just before a committed edit mutates the model.
        std::function<void()> onBeforeEdit;
    };

    CanvasView& View()
    {
        static CanvasView view;
        return view;
    }

    void NotifyEdited()
    {
        CanvasView& view = View();
        if (view.onEdited)
        {
            view.onEdited();
        }
    }

    void NotifyBeforeEdit()
    {
        CanvasView& view = View();
        if (view.onBeforeEdit)
        {
            view.onBeforeEdit();
        }
    }

    float ComputeScale(const CanvasView& view, int clientWidth, int clientHeight)
    {
        if (view.virtualWidth <= 0 || view.virtualHeight <= 0 || clientWidth <= 0 || clientHeight <= 0)
        {
            return 1.0f;
        }
        return std::min(static_cast<float>(clientWidth) / static_cast<float>(view.virtualWidth),
                        static_cast<float>(clientHeight) / static_cast<float>(view.virtualHeight));
    }

    POINT ClientToVirtualPoint(HWND hwnd, POINT clientPt)
    {
        const CanvasView& view = View();
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const float scale = ComputeScale(view, width, height);
        const int offsetX = static_cast<int>((static_cast<float>(width) - static_cast<float>(view.virtualWidth) * scale) / 2.0f);
        const int offsetY = static_cast<int>((static_cast<float>(height) - static_cast<float>(view.virtualHeight) * scale) / 2.0f);

        POINT virtualPt{};
        virtualPt.x = static_cast<int>(static_cast<float>(clientPt.x - offsetX) / scale);
        virtualPt.y = static_cast<int>(static_cast<float>(clientPt.y - offsetY) / scale);
        return virtualPt;
    }

    int MultiplierFromVirtual(const CanvasView& view, POINT virtualPt, GridData::Orientation orientation)
    {
        if (orientation == GridData::Orientation::Horizontal)
        {
            return virtualPt.y * GridData::Multiplier / view.virtualHeight;
        }
        return virtualPt.x * GridData::Multiplier / view.virtualWidth;
    }

    int HitTestResizer(const CanvasView& view, POINT virtualPt)
    {
        if (!view.grid)
        {
            return -1;
        }
        int best = -1;
        int bestDistance = std::numeric_limits<int>::max();
        const float scale = ComputeScale(view, view.clientWidth, view.clientHeight);
        const int threshold = std::max(1, static_cast<int>(std::lround(kResizerHitThreshold / std::max(scale, 0.01f))));
        const auto& resizers = view.grid->Resizers();
        for (size_t i = 0; i < resizers.size(); ++i)
        {
            const int position = view.grid->ResizerPosition(static_cast<int>(i));
            if (position < 0)
            {
                continue;
            }
            const bool horizontal = resizers[i].orientation == GridData::Orientation::Horizontal;
            const int axisPosition = horizontal
                                         ? position * view.virtualHeight / GridData::Multiplier
                                         : position * view.virtualWidth / GridData::Multiplier;
            const int mouse = horizontal ? virtualPt.y : virtualPt.x;
            const int distance = std::abs(mouse - axisPosition);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = static_cast<int>(i);
            }
        }
        return (bestDistance <= threshold) ? best : -1;
    }

    int HitTestZone(const CanvasView& view, POINT virtualPt)
    {
        if (!view.grid)
        {
            return -1;
        }
        const int mx = virtualPt.x * GridData::Multiplier / view.virtualWidth;
        const int my = virtualPt.y * GridData::Multiplier / view.virtualHeight;
        for (const auto& zone : view.grid->Zones())
        {
            if (zone.left <= mx && mx < zone.right && zone.top <= my && my < zone.bottom)
            {
                return zone.index;
            }
        }
        return -1;
    }

    std::vector<EditorCanvas::ZoneRect> CanvasModelZones(const CanvasView& view)
    {
        std::vector<EditorCanvas::ZoneRect> result;
        if (!view.canvasModel)
        {
            return result;
        }
        int index = 0;
        for (const auto& zone : view.canvasModel->zones)
        {
            result.push_back(EditorCanvas::ZoneRect{
                RECT{ zone.x, zone.y, zone.x + zone.width, zone.y + zone.height },
                index++
            });
        }
        return result;
    }

    int HitTestCanvasZone(const CanvasView& view, POINT virtualPt)
    {
        if (!view.canvasModel)
        {
            return -1;
        }
        for (size_t i = 0; i < view.canvasModel->zones.size(); ++i)
        {
            const auto& zone = view.canvasModel->zones[i];
            const RECT rect{ zone.x, zone.y, zone.x + zone.width, zone.y + zone.height };
            if (rect.left <= virtualPt.x && virtualPt.x < rect.right && rect.top <= virtualPt.y && virtualPt.y < rect.bottom)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    RECT CanvasHandleRect(const RECT& zone, int handle)
    {
        return CanvasMath::HandleRect(zone, static_cast<CanvasMath::Handle>(handle));
    }

    int HitTestCanvasHandle(const CanvasView& view, POINT virtualPt)
    {
        if (!view.canvasModel || view.selectedCanvasZone < 0 ||
            view.selectedCanvasZone >= static_cast<int>(view.canvasModel->zones.size()))
        {
            return CanvasMath::None;
        }
        const auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
        const RECT rect{ zone.x, zone.y, zone.x + zone.width, zone.y + zone.height };
        for (int handle = CanvasMath::NW; handle <= CanvasMath::W; ++handle)
        {
            if (CanvasMath::HandleHits(rect, static_cast<CanvasMath::Handle>(handle), virtualPt))
            {
                return handle;
            }
        }
        return CanvasMath::None;
    }

    RECT ClampToCanvas(RECT& rect, const CanvasView& view)
    {
        return CanvasMath::ClampToCanvas(rect, view.virtualWidth, view.virtualHeight);
    }

    int FindResizerForEdge(const CanvasView& view, int zoneIndex, bool horizontal, bool positiveSide)
    {
        if (!view.grid || zoneIndex < 0)
        {
            return -1;
        }
        const auto& zones = view.grid->Zones();
        if (zoneIndex >= static_cast<int>(zones.size()))
        {
            return -1;
        }
        const auto& zone = zones[zoneIndex];
        const int edgePos = horizontal
                                ? (positiveSide ? zone.top : zone.bottom)
                                : (positiveSide ? zone.left : zone.right);

        for (int i = 0; i < static_cast<int>(view.grid->Resizers().size()); ++i)
        {
            const auto& resizer = view.grid->Resizers()[i];
            const GridData::Orientation expected = horizontal ? GridData::Orientation::Horizontal : GridData::Orientation::Vertical;
            if (resizer.orientation != expected)
            {
                continue;
            }
            if (view.grid->ResizerPosition(i) != edgePos)
            {
                continue;
            }
            const auto& sideIndices = positiveSide ? resizer.positiveSideIndices : resizer.negativeSideIndices;
            if (std::find(sideIndices.begin(), sideIndices.end(), zoneIndex) != sideIndices.end())
            {
                return i;
            }
        }
        return -1;
    }

    RECT ResizeRect(const RECT& original, int handle, int dx, int dy)
    {
        return CanvasMath::Resize(original, static_cast<CanvasMath::Handle>(handle), dx, dy);
    }

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

        const Colors::ZoneColors colors = Colors::GetZoneColors();
        HBRUSH fillBrush = CreateSolidBrush(colors.primaryColor);
        HBRUSH borderBrush = CreateSolidBrush(colors.borderColor);
        HBRUSH highlightBrush = CreateSolidBrush(colors.highlightColor);
        HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
        HPEN borderPen = CreatePen(PS_SOLID, 1, colors.borderColor);
        HPEN highlightPen = CreatePen(PS_SOLID, 2, colors.highlightColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, borderPen));

        std::vector<EditorCanvas::ZoneRect> drawnZones;
        if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
        {
            for (const auto& zone : view.grid->Zones())
            {
                drawnZones.push_back(EditorCanvas::ZoneRect{
                    RECT{
                        zone.left * view.virtualWidth / GridData::Multiplier,
                        zone.top * view.virtualHeight / GridData::Multiplier,
                        zone.right * view.virtualWidth / GridData::Multiplier,
                        zone.bottom * view.virtualHeight / GridData::Multiplier
                    },
                    zone.index
                });
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
            DrawZoneLabel(dc, rect, zone.index, pixW, pixH, colors.numberColor);
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
        DeleteObject(highlightPen);
        DeleteObject(font);
        DeleteObject(fillBrush);
        DeleteObject(borderBrush);
        DeleteObject(highlightBrush);
    }

    void ShowContextMenu(HWND hwnd, POINT virtualPt)
    {
        CanvasView& view = View();
        if (view.mode == EditorCanvas::Mode::CanvasEdit)
        {
            if (!view.canvasModel)
            {
                return;
            }
            const int zoneIndex = HitTestCanvasZone(view, virtualPt);
            if (zoneIndex < 0)
            {
                return;
            }
            view.selectedCanvasZone = zoneIndex;

            HMENU menu = CreatePopupMenu();
            if (!menu)
            {
                return;
            }
            AppendMenuW(menu, MF_STRING, 1, L"Delete zone");
            POINT pt{};
            GetCursorPos(&pt);
            const UINT selected = static_cast<UINT>(TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, nullptr));
            DestroyMenu(menu);
            if (selected == 1 && view.canvasModel && view.selectedCanvasZone >= 0 &&
                view.selectedCanvasZone < static_cast<int>(view.canvasModel->zones.size()))
            {
                NotifyBeforeEdit();
                view.canvasModel->zones.erase(view.canvasModel->zones.begin() + view.selectedCanvasZone);
                view.selectedCanvasZone = -1;
                NotifyEdited();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return;
        }

        if (!view.grid)
        {
            return;
        }

        const int zoneIndex = HitTestZone(view, virtualPt);
        const int splitZone = zoneIndex;
        const bool canSplit = splitZone >= 0;
        const bool canMerge = view.selectedZones.size() >= 2;

        HMENU menu = CreatePopupMenu();
        if (!menu)
        {
            return;
        }
        AppendMenuW(menu, MF_STRING | (canSplit ? 0 : MF_GRAYED), 1, L"Split zone (2x2)");
        AppendMenuW(menu, MF_STRING | (canMerge ? 0 : MF_GRAYED), 2, L"Merge selected zones");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 3, L"Reset selection");

        POINT pt{};
        GetCursorPos(&pt);
        const UINT selected = static_cast<UINT>(TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, nullptr));
        DestroyMenu(menu);

        switch (selected)
        {
        case 1:
            if (canSplit)
            {
                NotifyBeforeEdit();
                view.grid->Split2x2(splitZone);
                view.selectedZones = { splitZone };
                NotifyEdited();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        case 2:
            if (canMerge)
            {
                NotifyBeforeEdit();
                view.grid->DoMerge(view.selectedZones);
                view.selectedZones.clear();
                NotifyEdited();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        case 3:
            view.selectedZones.clear();
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        default:
            break;
        }
    }

    LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        CanvasView& view = View();
        switch (msg)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            DrawView(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            {
                RECT client{};
                GetClientRect(hwnd, &client);
                view.clientWidth = client.right - client.left;
                view.clientHeight = client.bottom - client.top;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            const POINT pt = ClientToVirtualPoint(hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

            if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel)
            {
                SetFocus(hwnd);
                const int handle = HitTestCanvasHandle(view, pt);
                const int zone = HitTestCanvasZone(view, pt);
                if (handle != CanvasMath::None && view.selectedCanvasZone >= 0)
                {
                    NotifyBeforeEdit();
                    view.canvasInteraction = CanvasInteraction::Resize;
                    view.canvasResizeHandle = handle;
                    view.canvasDragAnchor = pt;
                    view.canvasDragOrigin = RECT{
                        view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].x,
                        view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].y,
                        view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].x + view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].width,
                        view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].y + view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)].height
                    };
                    SetCapture(hwnd);
                }
                else if (zone >= 0)
                {
                    NotifyBeforeEdit();
                    view.selectedCanvasZone = zone;
                    view.canvasInteraction = CanvasInteraction::Move;
                    view.canvasDragAnchor = pt;
                    view.canvasDragOrigin = RECT{
                        view.canvasModel->zones[static_cast<size_t>(zone)].x,
                        view.canvasModel->zones[static_cast<size_t>(zone)].y,
                        view.canvasModel->zones[static_cast<size_t>(zone)].x + view.canvasModel->zones[static_cast<size_t>(zone)].width,
                        view.canvasModel->zones[static_cast<size_t>(zone)].y + view.canvasModel->zones[static_cast<size_t>(zone)].height
                    };
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else
                {
                    view.selectedCanvasZone = -1;
                    view.canvasInteraction = CanvasInteraction::Draw;
                    view.canvasDragAnchor = pt;
                    view.canvasDrawing = true;
                    view.canvasDrawRect = RECT{ pt.x, pt.y, pt.x, pt.y };
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }

            if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
            {
                SetFocus(hwnd);
                const int resizer = HitTestResizer(view, pt);
                if (resizer >= 0)
                {
                    NotifyBeforeEdit();
                    view.dragResizer = resizer;
                    view.dragLastMultiplier = MultiplierFromVirtual(view, pt, view.grid->Resizers()[static_cast<size_t>(resizer)].orientation);
                    view.gridSnapshot = view.grid->Model();
                    SetCapture(hwnd);
                }
                else
                {
                    const int zone = HitTestZone(view, pt);
                    if (zone >= 0)
                    {
                        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        const auto existing = std::find(view.selectedZones.begin(), view.selectedZones.end(), zone);
                        if (ctrl)
                        {
                            if (existing != view.selectedZones.end())
                            {
                                view.selectedZones.erase(existing);
                            }
                            else
                            {
                                view.selectedZones.push_back(zone);
                            }
                        }
                        else
                        {
                            view.selectedZones = { zone };
                        }
                        InvalidateRect(hwnd, nullptr, TRUE);
                    }
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            const POINT pt = ClientToVirtualPoint(hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

            if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel)
            {
                const int dx = pt.x - view.canvasDragAnchor.x;
                const int dy = pt.y - view.canvasDragAnchor.y;
                if (view.canvasInteraction == CanvasInteraction::Move && view.selectedCanvasZone >= 0)
                {
                    RECT rect = view.canvasDragOrigin;
                    rect.left += dx;
                    rect.right += dx;
                    rect.top += dy;
                    rect.bottom += dy;
                    ClampToCanvas(rect, view);
                    auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
                    zone.x = rect.left;
                    zone.y = rect.top;
                    zone.width = rect.right - rect.left;
                    zone.height = rect.bottom - rect.top;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else if (view.canvasInteraction == CanvasInteraction::Resize && view.selectedCanvasZone >= 0)
                {
                    RECT rect = ResizeRect(view.canvasDragOrigin, view.canvasResizeHandle, dx, dy);
                    ClampToCanvas(rect, view);
                    auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
                    zone.x = rect.left;
                    zone.y = rect.top;
                    zone.width = rect.right - rect.left;
                    zone.height = rect.bottom - rect.top;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else if (view.canvasInteraction == CanvasInteraction::Draw && view.canvasDrawing)
                {
                    view.canvasDrawRect = RECT{ view.canvasDragAnchor.x, view.canvasDragAnchor.y, pt.x, pt.y };
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }

            if (view.dragResizer >= 0 && view.grid)
            {
                const GridData::Orientation orientation = view.grid->Resizers()[static_cast<size_t>(view.dragResizer)].orientation;
                const int multiplier = MultiplierFromVirtual(view, pt, orientation);
                const int delta = multiplier - view.dragLastMultiplier;
                if (delta != 0 && view.grid->CanDrag(view.dragResizer, delta))
                {
                    view.grid->Drag(view.dragResizer, delta);
                    view.dragLastMultiplier = multiplier;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel)
            {
                if (view.canvasInteraction == CanvasInteraction::Draw && view.canvasDrawing)
                {
                    RECT rect = CanvasMath::Normalize(view.canvasDrawRect);
                    view.canvasDrawing = false;
                    if (rect.right - rect.left >= CanvasMath::MinZoneSize && rect.bottom - rect.top >= CanvasMath::MinZoneSize)
                    {
                        NotifyBeforeEdit();
                        ClampToCanvas(rect, view);
                        view.canvasModel->zones.push_back(FancyZonesDataTypes::CanvasLayoutInfo::Rect{
                            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top
                        });
                        view.selectedCanvasZone = static_cast<int>(view.canvasModel->zones.size()) - 1;
                    }
                    view.canvasInteraction = CanvasInteraction::None;
                    if (GetCapture() == hwnd)
                    {
                        ReleaseCapture();
                    }
                    NotifyEdited();
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else if (view.canvasInteraction == CanvasInteraction::Move || view.canvasInteraction == CanvasInteraction::Resize)
                {
                    view.canvasInteraction = CanvasInteraction::None;
                    if (GetCapture() == hwnd)
                    {
                        ReleaseCapture();
                    }
                    NotifyEdited();
                }
                return 0;
            }

            if (view.dragResizer >= 0)
            {
                view.dragResizer = -1;
                ReleaseCapture();
                NotifyEdited();
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK:
            if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
            {
                const POINT pt = ClientToVirtualPoint(hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
                const int zone = HitTestZone(view, pt);
                if (zone >= 0)
                {
                    NotifyBeforeEdit();
                    view.grid->Split2x2(zone);
                    view.selectedZones = { zone };
                    NotifyEdited();
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;

        case WM_RBUTTONUP:
            if (view.mode == EditorCanvas::Mode::GridEdit || view.mode == EditorCanvas::Mode::CanvasEdit)
            {
                const POINT pt = ClientToVirtualPoint(hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
                ShowContextMenu(hwnd, pt);
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                if (EditorCanvas::CancelActiveOperation(hwnd))
                {
                    return 0;
                }
            }
            if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT)
            {
                if (EditorCanvas::IsDragging(hwnd))
                {
                    break;
                }
                if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel &&
                    view.selectedCanvasZone >= 0 && view.selectedCanvasZone < static_cast<int>(view.canvasModel->zones.size()))
                {
                    NotifyBeforeEdit();
                    auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
                    switch (wParam)
                    {
                    case VK_UP:    zone.y -= 1; zone.height += 1; break;
                    case VK_DOWN:  zone.height += 1; break;
                    case VK_LEFT:  zone.x -= 1; zone.width += 1; break;
                    case VK_RIGHT: zone.width += 1; break;
                    }
                    zone.width = std::max(1, zone.width);
                    zone.height = std::max(1, zone.height);
                    RECT rect{ zone.x, zone.y, zone.x + zone.width, zone.y + zone.height };
                    ClampToCanvas(rect, view);
                    zone.x = rect.left;
                    zone.y = rect.top;
                    zone.width = rect.right - rect.left;
                    zone.height = rect.bottom - rect.top;
                    NotifyEdited();
                    InvalidateRect(hwnd, nullptr, TRUE);
                    return 0;
                }
                if (view.mode == EditorCanvas::Mode::GridEdit && view.grid &&
                    view.selectedZones.size() == 1)
                {
                    const int zoneIndex = view.selectedZones[0];
                    int resizerIndex = -1;
                    int delta = 0;
                    switch (wParam)
                    {
                    case VK_UP:
                        resizerIndex = FindResizerForEdge(view, zoneIndex, true, true);
                        delta = -GridData::Multiplier / view.virtualHeight;
                        break;
                    case VK_DOWN:
                        resizerIndex = FindResizerForEdge(view, zoneIndex, true, false);
                        delta = GridData::Multiplier / view.virtualHeight;
                        break;
                    case VK_LEFT:
                        resizerIndex = FindResizerForEdge(view, zoneIndex, false, true);
                        delta = -GridData::Multiplier / view.virtualWidth;
                        break;
                    case VK_RIGHT:
                        resizerIndex = FindResizerForEdge(view, zoneIndex, false, false);
                        delta = GridData::Multiplier / view.virtualWidth;
                        break;
                    }
                    if (resizerIndex >= 0 && view.grid->CanDrag(resizerIndex, delta))
                    {
                        NotifyBeforeEdit();
                        view.grid->Drag(resizerIndex, delta);
                        NotifyEdited();
                        InvalidateRect(hwnd, nullptr, TRUE);
                    }
                    return 0;
                }
            }
            if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel &&
                (wParam == VK_DELETE || wParam == VK_BACK))
            {
                if (view.selectedCanvasZone >= 0 && view.selectedCanvasZone < static_cast<int>(view.canvasModel->zones.size()))
                {
                    NotifyBeforeEdit();
                    view.canvasModel->zones.erase(view.canvasModel->zones.begin() + view.selectedCanvasZone);
                    view.selectedCanvasZone = -1;
                    NotifyEdited();
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }
            break;

        case WM_SETCURSOR:
            if (view.mode == EditorCanvas::Mode::CanvasEdit && view.canvasModel)
            {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(hwnd, &cursor);
                const POINT pt = ClientToVirtualPoint(hwnd, cursor);
                const int handle = HitTestCanvasHandle(view, pt);
                if (handle != CanvasMath::None)
                {
                    switch (handle)
                    {
                    case CanvasMath::NW:
                    case CanvasMath::SE:
                        SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
                        break;
                    case CanvasMath::NE:
                    case CanvasMath::SW:
                        SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
                        break;
                    case CanvasMath::N:
                    case CanvasMath::S:
                        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                        break;
                    default:
                        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                        break;
                    }
                    return TRUE;
                }
                if (HitTestCanvasZone(view, pt) >= 0)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
                SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            }
            if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
            {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(hwnd, &cursor);
                const POINT pt = ClientToVirtualPoint(hwnd, cursor);
                const int resizer = HitTestResizer(view, pt);
                if (resizer >= 0)
                {
                    const bool horizontal = view.grid->Resizers()[static_cast<size_t>(resizer)].orientation == GridData::Orientation::Horizontal;
                    SetCursor(LoadCursorW(nullptr, horizontal ? IDC_SIZENS : IDC_SIZEWE));
                    return TRUE;
                }
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;

        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

namespace EditorCanvas
{
    HWND Create(HWND parent, HINSTANCE hInstance)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = &CanvasProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kCanvasClassName;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return nullptr;
        }

        return CreateWindowExW(0, kCanvasClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
                               0, 0, 0, 0, parent, nullptr, hInstance, nullptr);
    }

    void SetZones(HWND hwnd, int virtualWidth, int virtualHeight, std::vector<ZoneRect> zones)
    {
        CanvasView& view = View();
        view.mode = Mode::Preview;
        view.virtualWidth = virtualWidth;
        view.virtualHeight = virtualHeight;
        view.zones = std::move(zones);
        view.grid.reset();
        view.canvasModel = nullptr;
        view.selectedZones.clear();
        view.selectedCanvasZone = -1;
        view.canvasInteraction = CanvasInteraction::None;
        view.canvasDrawing = false;
        view.dragResizer = -1;
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void SetGridEdit(HWND hwnd, GridData::Grid grid, int virtualWidth, int virtualHeight)
    {
        CanvasView& view = View();
        view.mode = Mode::GridEdit;
        view.virtualWidth = std::max(1, virtualWidth);
        view.virtualHeight = std::max(1, virtualHeight);
        view.zones.clear();
        view.grid = std::move(grid);
        view.canvasModel = nullptr;
        view.selectedZones.clear();
        view.selectedCanvasZone = -1;
        view.canvasInteraction = CanvasInteraction::None;
        view.canvasDrawing = false;
        view.dragResizer = -1;
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void SetCanvasEdit(HWND hwnd, FancyZonesDataTypes::CanvasLayoutInfo* model)
    {
        CanvasView& view = View();
        view.mode = Mode::CanvasEdit;
        view.virtualWidth = model ? std::max(1, model->lastWorkAreaWidth) : 1600;
        view.virtualHeight = model ? std::max(1, model->lastWorkAreaHeight) : 900;
        view.zones.clear();
        view.grid.reset();
        view.canvasModel = model;
        view.selectedZones.clear();
        view.selectedCanvasZone = -1;
        view.canvasInteraction = CanvasInteraction::None;
        view.canvasDrawing = false;
        view.dragResizer = -1;
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    POINT ClientToVirtual(HWND hwnd, POINT clientPt)
    {
        return ClientToVirtualPoint(hwnd, clientPt);
    }

    void SetOnEdited(HWND hwnd, std::function<void()> callback)
    {
        (void)hwnd;
        CanvasView& view = View();
        view.onEdited = std::move(callback);
    }

    void SetOnBeforeEdit(HWND hwnd, std::function<void()> callback)
    {
        (void)hwnd;
        CanvasView& view = View();
        view.onBeforeEdit = std::move(callback);
    }

    bool IsDragging(HWND hwnd)
    {
        (void)hwnd;
        CanvasView& view = View();
        if (view.dragResizer >= 0)
        {
            return true;
        }
        if (view.canvasInteraction != CanvasInteraction::None)
        {
            return true;
        }
        if (view.canvasDrawing)
        {
            return true;
        }
        return false;
    }

    bool CancelActiveOperation(HWND hwnd)
    {
        CanvasView& view = View();
        bool cancelled = false;

        // Grid resizer drag: restore snapshot.
        if (view.dragResizer >= 0 && view.grid)
        {
            view.grid->Model() = view.gridSnapshot;
            view.grid->Reset();
            view.dragResizer = -1;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            cancelled = true;
        }

        // Canvas move/resize: restore origin position.
        if (view.canvasInteraction == CanvasInteraction::Move || view.canvasInteraction == CanvasInteraction::Resize)
        {
            if (view.canvasModel && view.selectedCanvasZone >= 0 &&
                view.selectedCanvasZone < static_cast<int>(view.canvasModel->zones.size()))
            {
                auto& zone = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
                zone.x = view.canvasDragOrigin.left;
                zone.y = view.canvasDragOrigin.top;
                zone.width = view.canvasDragOrigin.right - view.canvasDragOrigin.left;
                zone.height = view.canvasDragOrigin.bottom - view.canvasDragOrigin.top;
            }
            view.canvasInteraction = CanvasInteraction::None;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            cancelled = true;
        }
        // Canvas draw: discard in-progress draw rect.
        else if (view.canvasInteraction == CanvasInteraction::Draw && view.canvasDrawing)
        {
            view.canvasDrawing = false;
            view.canvasInteraction = CanvasInteraction::None;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            cancelled = true;
        }

        if (cancelled)
        {
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return cancelled;
    }
}
