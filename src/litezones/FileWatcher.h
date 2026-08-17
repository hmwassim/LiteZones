#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

// Watches a directory for changes to specific file names and posts a message
// to a window on the main thread. Uses overlapped ReadDirectoryChangesW so
// the thread can be woken instantly via a stop event.
class FileWatcher
{
public:
    FileWatcher() = default;
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Starts watching. Posts msg to hwnd when a watched file changes.
    bool Start(HWND hwnd, UINT msg, const std::wstring& directory, std::vector<std::wstring> watchNames);
    // Signals the thread to stop and waits for it to exit.
    void Stop();

private:
    static DWORD WINAPI ThreadProc(LPVOID param);
    DWORD Run();

    HWND m_hwnd = nullptr;
    UINT m_msg = 0;
    HANDLE m_dirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_thread = nullptr;
    HANDLE m_stopEvent = nullptr;
    OVERLAPPED m_overlapped{};
    std::vector<std::wstring> m_watchNames;
};
