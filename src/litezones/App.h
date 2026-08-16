#pragma once

#include <windows.h>

class App
{
public:
    explicit App(HINSTANCE hInstance);
    ~App();

    bool Init();
    int Run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateHiddenWindow();
    bool AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void UpdateTrayTip();
    void ToggleSnapping();
    void ToggleAutostart();
    bool IsAutostartEnabled() const;

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_snappingEnabled = true;
    bool m_autostart = false;
};
