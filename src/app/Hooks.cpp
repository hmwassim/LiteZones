#include "Hooks.h"

Hooks::Hooks(HWND targetWindow) :
    m_targetWindow(targetWindow)
{
}

Hooks::~Hooks()
{
    Stop();
}

bool Hooks::Start()
{
    if (!m_targetWindow)
    {
        return false;
    }
    s_targetWindow = m_targetWindow;

    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &Hooks::LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);

    m_moveStartHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZESTART, nullptr, &Hooks::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    m_moveEndHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZEEND, EVENT_SYSTEM_MOVESIZEEND, nullptr, &Hooks::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    m_destroyHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, &Hooks::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    return m_keyboardHook != nullptr
        && m_moveStartHook != nullptr
        && m_moveEndHook != nullptr
        && m_destroyHook != nullptr;
}

void Hooks::Stop()
{
    DisableLocationChangeTracking();
    DisableMouseButtonHook();

    if (m_moveStartHook)
    {
        UnhookWinEvent(m_moveStartHook);
        m_moveStartHook = nullptr;
    }
    if (m_moveEndHook)
    {
        UnhookWinEvent(m_moveEndHook);
        m_moveEndHook = nullptr;
    }
    if (m_destroyHook)
    {
        UnhookWinEvent(m_destroyHook);
        m_destroyHook = nullptr;
    }
    if (m_keyboardHook)
    {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }

    s_targetWindow = nullptr;
}

void Hooks::EnableLocationChangeTracking()
{
    if (!m_locationChangeHook)
    {
        m_locationChangeHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, &Hooks::WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
}

void Hooks::DisableLocationChangeTracking()
{
    if (m_locationChangeHook)
    {
        UnhookWinEvent(m_locationChangeHook);
        m_locationChangeHook = nullptr;
    }
}

void Hooks::EnableMouseButtonHook()
{
    if (!m_mouseHook)
    {
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &Hooks::LowLevelMouseProc, GetModuleHandleW(nullptr), 0);
    }
}

void Hooks::DisableMouseButtonHook()
{
    if (m_mouseHook)
    {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
}

LRESULT CALLBACK Hooks::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        const auto info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // The hook reports the concrete key (VK_LSHIFT/VK_RSHIFT, VK_LCONTROL/
        // VK_RCONTROL); normalize to the generic codes the rest of the code uses.
        DWORD vkCode = info->vkCode;
        if (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)
        {
            vkCode = VK_SHIFT;
        }
        else if (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)
        {
            vkCode = VK_CONTROL;
        }

        if (vkCode == VK_SHIFT || vkCode == VK_CONTROL)
        {
            const bool pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            PostMessageW(s_targetWindow, WM_PRIV_KEYSTATE, vkCode, pressed ? 1 : 0);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Hooks::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP ||
            wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP)
        {
            const bool down = (wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN);
            const UINT button = (wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP) ? VK_RBUTTON : VK_MBUTTON;
            PostMessageW(s_targetWindow, WM_PRIV_MOUSEBUTTON, button, down ? 1 : 0);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void CALLBACK Hooks::WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD eventThread, DWORD eventTime)
{
    (void)hook;
    (void)eventThread;
    (void)eventTime;
    (void)idChild;

    switch (event)
    {
    case EVENT_SYSTEM_MOVESIZESTART:
        PostMessageW(s_targetWindow, WM_PRIV_MOVESIZESTART, reinterpret_cast<WPARAM>(hwnd), 0);
        break;
    case EVENT_SYSTEM_MOVESIZEEND:
        PostMessageW(s_targetWindow, WM_PRIV_MOVESIZEEND, reinterpret_cast<WPARAM>(hwnd), 0);
        break;
    case EVENT_OBJECT_LOCATIONCHANGE:
        if (idObject == OBJID_WINDOW)
        {
            PostMessageW(s_targetWindow, WM_PRIV_LOCATIONCHANGE, reinterpret_cast<WPARAM>(hwnd), 0);
        }
        break;
    case EVENT_OBJECT_DESTROY:
        if (idObject == OBJID_WINDOW)
        {
            PostMessageW(s_targetWindow, WM_PRIV_WINDOWDESTROYED, reinterpret_cast<WPARAM>(hwnd), 0);
        }
        break;
    default:
        break;
    }
}

HWND Hooks::s_targetWindow = nullptr;
