#pragma once

#include <windows.h>

#include <algorithm>

// Pure geometry for the canvas layout editor, kept free of window plumbing so
// it can be unit-tested. All values are in the layout's virtual space
// (reference work-area pixels).
namespace CanvasMath
{
    constexpr int HandleSize = 10;
    constexpr int MinZoneSize = 20;

    enum Handle : int
    {
        None = -1,
        NW = 0,
        NE = 1,
        SW = 2,
        SE = 3,
        N = 4,
        E = 5,
        S = 6,
        W = 7
    };

    // Flips a possibly-dragged rectangle so left<=right and top<=bottom.
    inline RECT Normalize(const RECT& rect)
    {
        RECT normalized = rect;
        if (normalized.left > normalized.right)
        {
            std::swap(normalized.left, normalized.right);
        }
        if (normalized.top > normalized.bottom)
        {
            std::swap(normalized.top, normalized.bottom);
        }
        return normalized;
    }

    // Shifts a rect into the canvas and enforces the minimum zone size.
    inline RECT ClampToCanvas(const RECT& rect, int canvasWidth, int canvasHeight)
    {
        RECT result = rect;
        if (result.left < 0)
        {
            result.right = result.right - result.left;
            result.left = 0;
        }
        if (result.top < 0)
        {
            result.bottom = result.bottom - result.top;
            result.top = 0;
        }
        if (result.right > canvasWidth)
        {
            result.left = result.left - (result.right - canvasWidth);
            result.right = canvasWidth;
        }
        if (result.bottom > canvasHeight)
        {
            result.top = result.top - (result.bottom - canvasHeight);
            result.bottom = canvasHeight;
        }
        if (result.right - result.left < MinZoneSize)
        {
            result.right = result.left + MinZoneSize;
        }
        if (result.bottom - result.top < MinZoneSize)
        {
            result.bottom = result.top + MinZoneSize;
        }
        return result;
    }

    // A zone with a handle id (NW/NE/SW/SE/N/E/S/W) resized by (dx, dy),
    // preserving the minimum zone size. The result may still fall outside the
    // canvas; call ClampToCanvas afterwards.
    inline RECT Resize(const RECT& original, Handle handle, int dx, int dy)
    {
        RECT rect = original;
        const bool leftEdge = (handle == NW || handle == SW || handle == W);
        const bool rightEdge = (handle == NE || handle == SE || handle == E);
        const bool topEdge = (handle == NW || handle == NE || handle == N);
        const bool bottomEdge = (handle == SW || handle == SE || handle == S);
        if (leftEdge)
        {
            rect.left += dx;
        }
        if (rightEdge)
        {
            rect.right += dx;
        }
        if (topEdge)
        {
            rect.top += dy;
        }
        if (bottomEdge)
        {
            rect.bottom += dy;
        }
        if (rect.right - rect.left < MinZoneSize)
        {
            if (leftEdge)
            {
                rect.left = rect.right - MinZoneSize;
            }
            else
            {
                rect.right = rect.left + MinZoneSize;
            }
        }
        if (rect.bottom - rect.top < MinZoneSize)
        {
            if (topEdge)
            {
                rect.top = rect.bottom - MinZoneSize;
            }
            else
            {
                rect.bottom = rect.top + MinZoneSize;
            }
        }
        return rect;
    }

    // Square hit-region for a zone's resize handle.
    inline RECT HandleRect(const RECT& zone, Handle handle)
    {
        const int half = HandleSize / 2;
        const int cx = (zone.left + zone.right) / 2;
        const int cy = (zone.top + zone.bottom) / 2;
        int hx = cx;
        int hy = cy;
        switch (handle)
        {
        case NW: hx = zone.left; hy = zone.top; break;
        case NE: hx = zone.right; hy = zone.top; break;
        case SW: hx = zone.left; hy = zone.bottom; break;
        case SE: hx = zone.right; hy = zone.bottom; break;
        case N: hy = zone.top; break;
        case E: hx = zone.right; break;
        case S: hy = zone.bottom; break;
        case W: hx = zone.left; break;
        default: break;
        }
        return RECT{ hx - half, hy - half, hx + half, hy + half };
    }

    inline bool HandleHits(const RECT& zone, Handle handle, POINT pt)
    {
        const RECT hr = HandleRect(zone, handle);
        return hr.left <= pt.x && pt.x <= hr.right && hr.top <= pt.y && pt.y <= hr.bottom;
    }
}
