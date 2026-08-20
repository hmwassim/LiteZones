#include "FileWatcher.h"

#include <algorithm>

namespace
{
    constexpr DWORD kWatchBufferSize = 64 * 1024;
    constexpr DWORD kWatchFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
    constexpr DWORD kStopEventIndex = WAIT_OBJECT_0 + 1;
    constexpr DWORD kStopTimeoutMs = 500;
}

FileWatcher::~FileWatcher()
{
    Stop();
}

bool FileWatcher::Start(HWND hwnd, UINT msg, const std::wstring& directory, std::vector<std::wstring> watchNames)
{
    Stop();

    m_hwnd = hwnd;
    m_msg = msg;
    m_watchNames = std::move(watchNames);

    m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_stopEvent)
    {
        return false;
    }

    m_dirHandle = CreateFileW(directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (m_dirHandle == INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
        return false;
    }

    m_thread = CreateThread(nullptr, 0, &FileWatcher::ThreadProc, this, 0, nullptr);
    if (!m_thread)
    {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
        return false;
    }
    return true;
}

void FileWatcher::Stop()
{
    if (m_stopEvent)
    {
        SetEvent(m_stopEvent);
    }
    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        CancelIo(m_dirHandle);
    }
    if (m_thread)
    {
        const DWORD waitResult = WaitForSingleObject(m_thread, kStopTimeoutMs);
        CloseHandle(m_thread);
        m_thread = nullptr;

        if (waitResult != WAIT_OBJECT_0)
        {
            // Run() is still alive somewhere between WaitForMultipleObjects()
            // and its next touch of m_dirHandle/m_overlapped/m_hwnd (this
            // should only happen if CancelIo() can't complete promptly, e.g.
            // a config directory on a network share). Closing those handles
            // out from under the still-running thread would be a
            // use-after-free, so leak them instead: they're closed once the
            // process exits, and Start() always allocates fresh ones rather
            // than reusing these.
            m_dirHandle = INVALID_HANDLE_VALUE;
            m_stopEvent = nullptr;
            m_hwnd = nullptr;
            return;
        }
    }
    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
    }
    if (m_stopEvent)
    {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }
    ZeroMemory(&m_overlapped, sizeof(m_overlapped));
    m_hwnd = nullptr;
}

DWORD WINAPI FileWatcher::ThreadProc(LPVOID param)
{
    auto* self = static_cast<FileWatcher*>(param);
    return self->Run();
}

DWORD FileWatcher::Run()
{
    std::vector<BYTE> buffer(kWatchBufferSize);
    HANDLE events[2] = { m_overlapped.hEvent, m_stopEvent };

    while (true)
    {
        DWORD bytesReturned = 0;
        ZeroMemory(&m_overlapped, sizeof(m_overlapped));
        m_overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_overlapped.hEvent)
        {
            break;
        }
        events[0] = m_overlapped.hEvent;

        const BOOL ok = ReadDirectoryChangesW(m_dirHandle, buffer.data(), static_cast<DWORD>(buffer.size()), FALSE, kWatchFilter, &bytesReturned, &m_overlapped, nullptr);
        if (!ok && GetLastError() != ERROR_IO_PENDING)
        {
            CloseHandle(m_overlapped.hEvent);
            break;
        }

        const DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

        if (waitResult == kStopEventIndex)
        {
            CancelIo(m_dirHandle);
            GetOverlappedResult(m_dirHandle, &m_overlapped, &bytesReturned, TRUE);
            CloseHandle(m_overlapped.hEvent);
            break;
        }

        if (waitResult != WAIT_OBJECT_0)
        {
            CloseHandle(m_overlapped.hEvent);
            break;
        }

        if (!GetOverlappedResult(m_dirHandle, &m_overlapped, &bytesReturned, FALSE) || bytesReturned == 0)
        {
            CloseHandle(m_overlapped.hEvent);
            continue;
        }

        CloseHandle(m_overlapped.hEvent);

        bool matched = false;
        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
        while (true)
        {
            const size_t charCount = info->FileNameLength / sizeof(wchar_t);
            std::wstring name(info->FileName, charCount);
            if (std::find(m_watchNames.begin(), m_watchNames.end(), name) != m_watchNames.end())
            {
                matched = true;
                break;
            }
            if (info->NextEntryOffset == 0)
            {
                break;
            }
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
        }

        if (matched && m_hwnd)
        {
            PostMessageW(m_hwnd, m_msg, 0, 0);
        }
    }
    return 0;
}
