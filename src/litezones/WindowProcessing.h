#pragma once

#include <windows.h>

struct SettingsData;

namespace WindowProcessing
{
    bool IsProcessableManually(HWND window, const SettingsData& settings) noexcept;
}
