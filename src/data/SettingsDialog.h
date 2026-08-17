#pragma once

#include <windows.h>

namespace SettingsDialog
{
    // Shows the settings dialog as a modal window. Returns true if the user
    // clicked OK (settings have already been saved to the singleton).
    bool Show(HWND owner, HINSTANCE hInstance);
}
