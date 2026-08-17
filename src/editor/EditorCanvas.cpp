#include "EditorCanvasInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace EditorCanvasInternal
{
    constexpr wchar_t kCanvasClassName[] = L"LiteZonesEditorCanvas";
    constexpr int kResizerHitThreshold = 8;

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

    void NotifyHint(const wchar_t* message)
    {
        CanvasView& view = View();
        if (view.onHint)
        {
            view.onHint(message);
        }
    }

    float ComputeScale(const CanvasView& view, int width, int height)
    {
        if (view.virtualWidth <= 0 || view.virtualHeight <= 0 || width <= 0 || height <= 0)
        {
            return 1.0f;
        }
        const float sx = static_cast<float>(width) / static_cast<float>(view.virtualWidth);
        const float sy = static_cast<float>(height) / static_cast<float>(view.virtualHeight);
        return std::min(sx, sy);
    }

    POINT ClientToVirtualPoint(HWND hwnd, POINT clientPt)
    {
        const CanvasView& view = View();
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const float scale = ComputeScale(view, width, height);
        const float offsetX = (static_cast<float>(width) - static_cast<float>(view.virtualWidth) * scale) / 2.0f;
        const float offsetY = (static_cast<float>(height) - static_cast<float>(view.virtualHeight) * scale) / 2.0f;
        POINT virtualPt{};
        virtualPt.x = static_cast<LONG>((static_cast<float>(clientPt.x) - offsetX) / scale);
        virtualPt.y = static_cast<LONG>((static_cast<float>(clientPt.y) - offsetY) / scale);
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
        const float scale = ComputeScale(view, view.clientWidth, view.clientHeight);
        const int threshold = static_cast<int>(kResizerHitThreshold / scale);
        const auto& resizers = view.grid->Resizers();
        const auto& zones = view.grid->Zones();
        int bestIndex = -1;
        int bestDist = threshold + 1;
        for (size_t i = 0; i < resizers.size(); ++i)
        {
            const auto& r = resizers[i];
            const int pos = view.grid->ResizerPosition(static_cast<int>(i));
            if (pos < 0)
            {
                continue;
            }
            const int posPx = (r.orientation == GridData::Orientation::Horizontal)
                ? pos * view.virtualHeight / GridData::Multiplier
                : pos * view.virtualWidth / GridData::Multiplier;
            const int dist = (r.orientation == GridData::Orientation::Horizontal)
                ? std::abs(virtualPt.y - posPx)
                : std::abs(virtualPt.x - posPx);
            int beginPx = 0;
            int endPx = 0;
            for (const int idx : r.negativeSideIndices)
            {
                if (idx >= 0 && idx < static_cast<int>(zones.size()))
                {
                    const auto& z = zones[static_cast<size_t>(idx)];
                    if (r.orientation == GridData::Orientation::Horizontal)
                    {
                        beginPx = std::max(beginPx, z.left * view.virtualWidth / GridData::Multiplier);
                        endPx = std::max(endPx, z.right * view.virtualWidth / GridData::Multiplier);
                    }
                    else
                    {
                        beginPx = std::max(beginPx, z.top * view.virtualHeight / GridData::Multiplier);
                        endPx = std::max(endPx, z.bottom * view.virtualHeight / GridData::Multiplier);
                    }
                }
            }
            for (const int idx : r.positiveSideIndices)
            {
                if (idx >= 0 && idx < static_cast<int>(zones.size()))
                {
                    const auto& z = zones[static_cast<size_t>(idx)];
                    if (r.orientation == GridData::Orientation::Horizontal)
                    {
                        beginPx = std::max(beginPx, z.left * view.virtualWidth / GridData::Multiplier);
                        endPx = std::max(endPx, z.right * view.virtualWidth / GridData::Multiplier);
                    }
                    else
                    {
                        beginPx = std::max(beginPx, z.top * view.virtualHeight / GridData::Multiplier);
                        endPx = std::max(endPx, z.bottom * view.virtualHeight / GridData::Multiplier);
                    }
                }
            }
            const bool inRange = (r.orientation == GridData::Orientation::Horizontal)
                ? (virtualPt.x >= beginPx && virtualPt.x <= endPx)
                : (virtualPt.y >= beginPx && virtualPt.y <= endPx);
            if (dist < bestDist && inRange)
            {
                bestDist = dist;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestIndex;
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
            if (mx >= zone.left && mx <= zone.right && my >= zone.top && my <= zone.bottom)
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
        result.reserve(view.canvasModel->zones.size());
        int index = 0;
        for (const auto& z : view.canvasModel->zones)
        {
            result.push_back(EditorCanvas::ZoneRect{
                RECT{ z.x, z.y, z.x + z.width, z.y + z.height },
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
        for (int i = static_cast<int>(view.canvasModel->zones.size()) - 1; i >= 0; --i)
        {
            const auto& z = view.canvasModel->zones[static_cast<size_t>(i)];
            if (virtualPt.x >= z.x && virtualPt.x <= z.x + z.width &&
                virtualPt.y >= z.y && virtualPt.y <= z.y + z.height)
            {
                return i;
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
        const auto& z = view.canvasModel->zones[static_cast<size_t>(view.selectedCanvasZone)];
        const RECT zoneRect{ z.x, z.y, z.x + z.width, z.y + z.height };
        for (int handle = CanvasMath::NW; handle <= CanvasMath::W; ++handle)
        {
            if (CanvasMath::HandleHits(zoneRect, static_cast<CanvasMath::Handle>(handle), virtualPt))
            {
                return handle;
            }
        }
        return CanvasMath::None;
    }

    void ClampToCanvas(RECT& rect, const CanvasView& view)
    {
        rect = CanvasMath::ClampToCanvas(rect, view.virtualWidth, view.virtualHeight);
    }

    int FindResizerForEdge(const CanvasView& view, int zoneIndex, bool orientation, bool positive)
    {
        if (!view.grid)
        {
            return -1;
        }
        const auto& zones = view.grid->Zones();
        if (zoneIndex < 0 || zoneIndex >= static_cast<int>(zones.size()))
        {
            return -1;
        }
        const auto& zone = zones[static_cast<size_t>(zoneIndex)];
        const int edge = orientation
            ? (positive ? zone.top : zone.bottom)
            : (positive ? zone.left : zone.right);
        const auto& resizers = view.grid->Resizers();
        int bestIndex = -1;
        int bestDist = std::numeric_limits<int>::max();
        for (size_t i = 0; i < resizers.size(); ++i)
        {
            const auto& r = resizers[i];
            const bool matchesOrientation = orientation
                ? (r.orientation == GridData::Orientation::Horizontal)
                : (r.orientation == GridData::Orientation::Vertical);
            if (!matchesOrientation)
            {
                continue;
            }
            const int pos = view.grid->ResizerPosition(static_cast<int>(i));
            if (pos < 0)
            {
                continue;
            }
            const int dist = std::abs(pos - edge);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestIndex;
    }

    RECT ResizeRect(const RECT& original, int handle, int dx, int dy)
    {
        return CanvasMath::Resize(original, static_cast<CanvasMath::Handle>(handle), dx, dy);
    }
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
        wc.lpszClassName = EditorCanvasInternal::kCanvasClassName;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return nullptr;
        }

        return CreateWindowExW(0, EditorCanvasInternal::kCanvasClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
                               0, 0, 0, 0, parent, nullptr, hInstance, nullptr);
    }

    void SetSettings(HWND hwnd, const SettingsData& settings)
    {
        (void)hwnd;
        EditorCanvasInternal::View().settings = &settings;
    }

    void SetZones(HWND hwnd, int virtualWidth, int virtualHeight, std::vector<ZoneRect> zones)
    {
        using namespace EditorCanvasInternal;
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
        using namespace EditorCanvasInternal;
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

    void SetCanvasEdit(HWND hwnd, LiteZonesTypes::CanvasLayoutInfo* model)
    {
        using namespace EditorCanvasInternal;
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
        return EditorCanvasInternal::ClientToVirtualPoint(hwnd, clientPt);
    }

    void SetOnEdited(HWND hwnd, std::function<void()> callback)
    {
        (void)hwnd;
        EditorCanvasInternal::View().onEdited = std::move(callback);
    }

    void SetOnBeforeEdit(HWND hwnd, std::function<void()> callback)
    {
        (void)hwnd;
        EditorCanvasInternal::View().onBeforeEdit = std::move(callback);
    }

    void SetOnHint(HWND hwnd, std::function<void(const wchar_t*)> callback)
    {
        (void)hwnd;
        EditorCanvasInternal::View().onHint = std::move(callback);
    }

    bool IsDragging(HWND hwnd)
    {
        (void)hwnd;
        EditorCanvasInternal::CanvasView& view = EditorCanvasInternal::View();
        if (view.dragResizer >= 0)
        {
            return true;
        }
        if (view.canvasInteraction != EditorCanvasInternal::CanvasInteraction::None)
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
        using namespace EditorCanvasInternal;
        CanvasView& view = View();
        bool cancelled = false;

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
