#pragma once

#include "GridData.h"

#include <windows.h>

#include <functional>
#include <vector>

// Child window that renders zones and supports in-place grid editing. Zones are
// given in a virtual coordinate space and scaled/letterboxed into the client
// area. In grid-edit mode the window holds a GridData::Grid (over an
// editor-owned model) and handles separator dragging, double-click splitting,
// zone selection and merging via a right-click menu.
namespace EditorCanvas
{
    enum class Mode
    {
        Preview,   // read-only zone preview
        GridEdit,  // in-place grid separator/cell editing
        CanvasEdit // draw/move/resize canvas zones
    };

    struct ZoneRect
    {
        RECT rect;
        int index;
    };

    HWND Create(HWND parent, HINSTANCE hInstance);

    // Read-only preview of zones in a virtualWidth x virtualHeight space.
    void SetZones(HWND hwnd, int virtualWidth, int virtualHeight, std::vector<ZoneRect> zones);

    // Enters grid-edit mode. The GridData::Grid references a model owned by the
    // caller; edits are written straight back into it. virtualWidth/Height are
    // the preview's virtual space (normally the selected monitor's work area).
    void SetGridEdit(HWND hwnd, GridData::Grid grid, int virtualWidth = 1600, int virtualHeight = 900);

    // Enters canvas-edit mode. The model is owned by the caller; drawing,
    // moving and resizing write straight back into it.
    void SetCanvasEdit(HWND hwnd, FancyZonesDataTypes::CanvasLayoutInfo* model);

    // Client pixel -> virtual-space point (inverse of the letterbox mapping).
    POINT ClientToVirtual(HWND hwnd, POINT clientPt);

    // Callback fired on every committed edit (drag-end, split, merge, delete).
    // Used by EditorWindow to track dirty state for mouse-driven changes.
    void SetOnEdited(HWND hwnd, std::function<void()> callback);
}
