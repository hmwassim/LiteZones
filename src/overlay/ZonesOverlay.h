#pragma once

#include "Colors.h"
#include "LayoutEngine.h"

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

#include <vector>

// Renders the zone layout into a work area's tool window using Direct2D.
// All coordinates are in work-area (window client) space.
class ZonesOverlay
{
public:
    explicit ZonesOverlay(HWND window);
    ~ZonesOverlay();

    ZonesOverlay(const ZonesOverlay&) = delete;
    ZonesOverlay& operator=(const ZonesOverlay&) = delete;

    // Rebuilds the scene from the given zones and (optionally) highlighted ones.
    void DrawActiveZoneSet(const ZonesMap& zones, const ZoneIndexSet& highlightZones, const Colors::ZoneColors& colors, bool showZoneText);
    void Show();
    void Hide();
    bool PreWarm();

private:
    struct DrawableRect
    {
        D2D1_RECT_F rect{};
        int id = 0;
        bool highlighted = false;
    };

    bool EnsureResources();
    void Render();

    HWND m_window = nullptr;
    ID2D1Factory* m_d2dFactory = nullptr;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    IDWriteFactory* m_writeFactory = nullptr;
    std::vector<DrawableRect> m_rects;
    Colors::ZoneColors m_colors{};
    bool m_showZoneText = false;
    float m_dpiScale = 1.f;
};
