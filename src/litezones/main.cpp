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
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app(hInstance);
    if (!app.Init())
    {
        CloseHandle(mutex);
        return 1;
    }

    const int result = app.Run();
    CloseHandle(mutex);
    return result;
}
