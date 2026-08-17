#include "EditorCanvasInternal.h"

#include "EditorCanvas.h"

#include <windowsx.h>

#include <algorithm>

namespace EditorCanvasInternal
{
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
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using namespace EditorCanvasInternal;
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
                    view.canvasModel->zones.push_back(LiteZonesTypes::CanvasLayoutInfo::Rect{
                        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top
                    });
                    view.selectedCanvasZone = static_cast<int>(view.canvasModel->zones.size()) - 1;
                }
                else
                {
                    NotifyHint(L"Zone too small, minimum 20x20.");
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
        if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            if (view.mode == EditorCanvas::Mode::GridEdit && view.grid)
            {
                view.selectedZones.clear();
                const int count = static_cast<int>(view.grid->Zones().size());
                for (int i = 0; i < count; ++i)
                {
                    view.selectedZones.push_back(i);
                }
                InvalidateRect(hwnd, nullptr, TRUE);
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
