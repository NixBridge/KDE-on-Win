// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

// Holding the named object (not owning the mutex) identifies a live instance.
class WindowsInstanceLock {
public:
    explicit WindowsInstanceLock(const wchar_t* name)
    {
        if (name) {
            handle = CreateMutexW(nullptr, FALSE, name);
            exists = GetLastError() == ERROR_ALREADY_EXISTS;
        }
    }
    ~WindowsInstanceLock()
    {
        if (handle)
            CloseHandle(handle);
    }
    WindowsInstanceLock(const WindowsInstanceLock&) = delete;
    WindowsInstanceLock& operator=(const WindowsInstanceLock&) = delete;
    bool valid() const { return handle != nullptr; }
    bool duplicate() const { return exists; }

private:
    HANDLE handle = nullptr;
    bool exists = false;
};
