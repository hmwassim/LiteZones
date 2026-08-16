#pragma once

#include "Hooks.h"
#include "Settings.h"
#include "WorkAreaManager.h"

#include <windows.h>

#include <memory>

class DragController;
class FileWatcher;
class KeyboardSnap;

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
    void ReloadConfig();
    void OpenConfigFolder();
    void ReloadWorkAreas();
    bool IsAutostartEnabled() const;

    void HandleMoveSizeStart(HWND window);
    void HandleMoveSizeEnd();
    void HandleMoveSizeUpdate();
    void HandleWindowDestroyed(HWND window);
    void HandleSnapHotkey(DWORD vkCode);
    void HandleWindowCreated(HWND window);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_snappingEnabled = true;
    bool m_autostart = false;

    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::unique_ptr<DragController> m_dragController;
    std::unique_ptr<KeyboardSnap> m_keyboardSnap;
    std::unique_ptr<Hooks> m_hooks;
    WorkAreaManager m_workAreaManager;
};
