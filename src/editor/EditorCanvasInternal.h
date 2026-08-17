#pragma once

#include "EditorCanvas.h"

#include "CanvasMath.h"
#include "GridData.h"
#include "Settings.h"

#include <functional>
#include <optional>
#include <vector>

// Internal shared state for the EditorCanvas implementation files.
// Not part of the public API.
namespace EditorCanvasInternal
{
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

        LiteZonesTypes::CanvasLayoutInfo* canvasModel = nullptr;
        int selectedCanvasZone = -1;
        CanvasInteraction canvasInteraction = CanvasInteraction::None;
        int canvasResizeHandle = CanvasMath::None;
        RECT canvasDragOrigin{};
        POINT canvasDragAnchor{};
        RECT canvasDrawRect{};
        bool canvasDrawing = false;

        LiteZonesTypes::GridLayoutInfo gridSnapshot{};

        std::function<void()> onEdited;
        std::function<void()> onBeforeEdit;
        std::function<void(const wchar_t*)> onHint;

        const SettingsData* settings = nullptr;
    };

    CanvasView& View();

    void NotifyEdited();
    void NotifyBeforeEdit();
    void NotifyHint(const wchar_t* message);

    float ComputeScale(const CanvasView& view, int width, int height);
    POINT ClientToVirtualPoint(HWND hwnd, POINT clientPt);
    int MultiplierFromVirtual(const CanvasView& view, POINT virtualPt, GridData::Orientation orientation);

    int HitTestResizer(const CanvasView& view, POINT virtualPt);
    int HitTestZone(const CanvasView& view, POINT virtualPt);
    std::vector<EditorCanvas::ZoneRect> CanvasModelZones(const CanvasView& view);
    int HitTestCanvasZone(const CanvasView& view, POINT virtualPt);
    RECT CanvasHandleRect(const RECT& zone, int handle);
    int HitTestCanvasHandle(const CanvasView& view, POINT virtualPt);
    void ClampToCanvas(RECT& rect, const CanvasView& view);
    int FindResizerForEdge(const CanvasView& view, int zoneIndex, bool orientation, bool positive);
    RECT ResizeRect(const RECT& original, int handle, int dx, int dy);

    void DrawTextCenter(HDC dc, const RECT& rect, int value, COLORREF color);
    void DrawZoneLabel(HDC dc, const RECT& screenRect, int index, int pixelW, int pixelH, COLORREF color);
    void DrawView(HWND hwnd, HDC dc);

    void ShowContextMenu(HWND hwnd, POINT virtualPt);
}
