#pragma once

#include <windows.h>

namespace WindowProcessing
{
    // True if the window can be snapped by the user dragging it (mouse snap).
    bool IsProcessableManually(HWND window) noexcept;
}
