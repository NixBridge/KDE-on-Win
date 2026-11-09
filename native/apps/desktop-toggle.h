// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <vector>
#include <string>

inline bool ordinaryDesktopWindowStyle(LONG_PTR style, LONG_PTR extended)
{
    return (style & WS_SYSMENU) && !(extended & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE));
}

class DesktopToggle {
    struct Entry {
        HWND window;
        DWORD pid;
    };
    std::vector<Entry> minimized;
    std::wstring property
        = L"KDE-on-Win.Desktop-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());

public:
    ~DesktopToggle() { restore(); }
    bool active() const { return !minimized.empty(); }
    bool minimize(HWND window)
    {
        DWORD pid = 0;
        if (!IsWindow(window) || IsIconic(window) || !GetWindowThreadProcessId(window, &pid))
            return false;
        // A property disappears when an HWND is destroyed, unlike its numeric ID.
        if (!SetPropW(window, property.c_str(), reinterpret_cast<HANDLE>(1)))
            return false;
        if (!ShowWindowAsync(window, SW_MINIMIZE)) {
            RemovePropW(window, property.c_str());
            return false;
        }
        minimized.push_back({ window, pid });
        return true;
    }
    void restore()
    {
        for (const auto& entry : minimized) {
            DWORD pid = 0;
            GetWindowThreadProcessId(entry.window, &pid);
            if (pid == entry.pid && GetPropW(entry.window, property.c_str())) {
                RemovePropW(entry.window, property.c_str());
                // Queue restore even if the preceding minimize is still pending.
                ShowWindowAsync(entry.window, SW_RESTORE);
            }
        }
        minimized.clear();
    }
    void toggle()
    {
        if (active()) {
            restore();
            return;
        }
        EnumWindows(
            [](HWND window, LPARAM data) -> BOOL {
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                const auto ex = GetWindowLongPtrW(window, GWL_EXSTYLE);
                if (!IsWindowVisible(window) || pid == GetCurrentProcessId() || GetWindow(window, GW_OWNER)
                    || !ordinaryDesktopWindowStyle(GetWindowLongPtrW(window, GWL_STYLE), ex))
                    return TRUE;
                wchar_t name[128] {};
                GetClassNameW(window, name, 128);
                if (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Progman") || !wcscmp(name, L"WorkerW"))
                    return TRUE;
                // Plasma desktop/panel HWNDs do not have WS_CAPTION; keep all its surfaces out anyway.
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (process) {
                    wchar_t path[32768] {};
                    DWORD length = 32768;
                    bool shell = QueryFullProcessImageNameW(process, 0, path, &length)
                        && (_wcsicmp(wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path, L"plasmashell.exe") == 0);
                    CloseHandle(process);
                    if (shell)
                        return TRUE;
                }
                reinterpret_cast<DesktopToggle*>(data)->minimize(window);
                return TRUE;
            },
            reinterpret_cast<LPARAM>(this));
    }
};
