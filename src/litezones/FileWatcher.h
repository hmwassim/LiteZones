#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

// Watches a directory for changes to specific file names and posts a message
// to a window on the main thread. Implemented with ReadDirectoryChangesW.
class FileWatcher
{
public:
    using ChangeCallback = std::function<void()>;

    FileWatcher() = default;
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Starts watching. Posts msg to hwnd when a watched file changes.
    bool Start(HWND hwnd, UINT msg, const std::wstring& directory, std::vector<std::wstring> watchNames);
    // Stops the watcher thread and closes the directory handle.
    void Stop();

private:
    static DWORD WINAPI ThreadProc(LPVOID param);
    DWORD Run();

    HWND m_hwnd = nullptr;
    UINT m_msg = 0;
    HANDLE m_dirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_thread = nullptr;
    std::vector<std::wstring> m_watchNames;
};
