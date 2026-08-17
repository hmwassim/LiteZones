#include "App.h"

#include <windows.h>

namespace
{
    constexpr wchar_t kInstanceMutexName[] = L"Local\\LiteZones_InstanceMutex";
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kInstanceMutexName);
    if (!mutex)
    {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        MessageBoxW(nullptr, L"LiteZones is already running.", L"LiteZones", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app(hInstance);
    if (!app.Init())
    {
        CloseHandle(mutex);
        MessageBoxW(nullptr, L"LiteZones failed to initialize.\n\nPossible causes:\n- Another instance may be interfering\n- Required system hooks could not be installed\n- Config directory could not be created",
                     L"LiteZones", MB_OK | MB_ICONERROR);
        return 1;
    }

    const int result = app.Run();
    CloseHandle(mutex);
    return result;
}
