#pragma once

#include <windows.h>

constexpr UINT WM_PRIV_MOVESIZESTART = WM_APP + 10;
constexpr UINT WM_PRIV_MOVESIZEEND = WM_APP + 11;
constexpr UINT WM_PRIV_LOCATIONCHANGE = WM_APP + 12;
constexpr UINT WM_PRIV_WINDOWDESTROYED = WM_APP + 13;
constexpr UINT WM_PRIV_KEYSTATE = WM_APP + 14;
constexpr UINT WM_PRIV_MOUSEBUTTON = WM_APP + 15;

class Hooks
{
public:
    explicit Hooks(HWND targetWindow);
    ~Hooks();

    Hooks(const Hooks&) = delete;
    Hooks& operator=(const Hooks&) = delete;

    bool Start();
    void Stop();

    // Adds/removes the EVENT_OBJECT_LOCATIONCHANGE hook (kept active only while dragging).
    void EnableLocationChangeTracking();
    void DisableLocationChangeTracking();

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD eventThread, DWORD eventTime);

    static HWND s_targetWindow;

    HWND m_targetWindow = nullptr;
    HHOOK m_keyboardHook = nullptr;
    HHOOK m_mouseHook = nullptr;
    HWINEVENTHOOK m_moveStartHook = nullptr;
    HWINEVENTHOOK m_moveEndHook = nullptr;
    HWINEVENTHOOK m_destroyHook = nullptr;
    HWINEVENTHOOK m_locationChangeHook = nullptr;
};
