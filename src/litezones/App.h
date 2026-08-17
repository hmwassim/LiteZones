#pragma once

#include "Hooks.h"
#include "Settings.h"
#include "TrayService.h"
#include "WorkAreaManager.h"

#include <windows.h>

#include <memory>

class DragController;
class EditorWindow;
class FileWatcher;
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
    void ToggleSnapping();
    void ReloadConfig();
    void OpenConfigFolder();
    void ReloadWorkAreas(bool forceRelayout = true);
    void CycleLayoutOnMonitor();
    void OpenLayoutEditor();

    void HandleMoveSizeStart(HWND window);
    void HandleMoveSizeEnd();
    void HandleMoveSizeUpdate();
    void HandleWindowDestroyed(HWND window);
    void Exit();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_snappingEnabled = true;

    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::unique_ptr<DragController> m_dragController;
    std::unique_ptr<Hooks> m_hooks;
    std::unique_ptr<EditorWindow> m_editor;
    WorkAreaManager m_workAreaManager;
    TrayService m_tray;
};
