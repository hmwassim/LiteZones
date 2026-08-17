#pragma once

#include "GridData.h"

#include <windows.h>

#include <functional>
#include <vector>

struct SettingsData;

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

    void SetSettings(HWND hwnd, const SettingsData& settings);

    // Read-only preview of zones in a virtualWidth x virtualHeight space.
    void SetZones(HWND hwnd, int virtualWidth, int virtualHeight, std::vector<ZoneRect> zones);

    // Enters grid-edit mode. The GridData::Grid references a model owned by the
    // caller; edits are written straight back into it. virtualWidth/Height are
    // the preview's virtual space (normally the selected monitor's work area).
    void SetGridEdit(HWND hwnd, GridData::Grid grid, int virtualWidth = 1600, int virtualHeight = 900);

    // Enters canvas-edit mode. The model is owned by the caller; drawing,
    // moving and resizing write straight back into it.
    void SetCanvasEdit(HWND hwnd, LiteZonesTypes::CanvasLayoutInfo* model);

    // Client pixel -> virtual-space point (inverse of the letterbox mapping).
    POINT ClientToVirtual(HWND hwnd, POINT clientPt);

    // Callback fired on every committed edit (drag-end, split, merge, delete).
    // Used by EditorWindow to track dirty state for mouse-driven changes.
    void SetOnEdited(HWND hwnd, std::function<void()> callback);

    // Callback fired just before a committed edit mutates the model.
    // Used by EditorWindow to snapshot undo state.
    void SetOnBeforeEdit(HWND hwnd, std::function<void()> callback);

    // Returns true if the canvas has an active mouse gesture (resizer drag,
    // zone move/resize, or zone draw).
    bool IsDragging(HWND hwnd);

    // Reverts the in-progress gesture (if any) without committing it.
    // Returns true if an operation was cancelled.
    bool CancelActiveOperation(HWND hwnd);
}
