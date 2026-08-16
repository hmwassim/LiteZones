#include "FileWatcher.h"

#include <algorithm>

namespace
{
    constexpr DWORD kWatchBufferSize = 64 * 1024;
    constexpr DWORD kWatchFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
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

    m_dirHandle = CreateFileW(directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (m_dirHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    m_thread = CreateThread(nullptr, 0, &FileWatcher::ThreadProc, this, 0, nullptr);
    if (!m_thread)
    {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

void FileWatcher::Stop()
{
    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        // Closing the directory handle makes ReadDirectoryChangesW return.
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
    }
    if (m_thread)
    {
        WaitForSingleObject(m_thread, 2000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
}

DWORD WINAPI FileWatcher::ThreadProc(LPVOID param)
{
    auto* self = static_cast<FileWatcher*>(param);
    return self->Run();
}

DWORD FileWatcher::Run()
{
    std::vector<BYTE> buffer(kWatchBufferSize);
    while (true)
    {
        DWORD bytesReturned = 0;
        const BOOL ok = ReadDirectoryChangesW(m_dirHandle, buffer.data(), static_cast<DWORD>(buffer.size()), FALSE, kWatchFilter, &bytesReturned, nullptr, nullptr);
        if (!ok)
        {
            break;
        }
        if (bytesReturned == 0)
        {
            continue;
        }

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
