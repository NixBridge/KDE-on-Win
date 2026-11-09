// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>

// This policy applies only to the wallpaper HWND, never panels or popups.
inline bool constrainWindowsDesktopStacking(UINT message, LPARAM parameter, qintptr *result)
{
    if (message == WM_WINDOWPOSCHANGING) {
        auto position = reinterpret_cast<WINDOWPOS *>(parameter);
        position->hwndInsertAfter = HWND_BOTTOM;
        position->flags = (position->flags & ~SWP_NOZORDER) | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    } else if (message == WM_MOUSEACTIVATE) {
        *result = MA_NOACTIVATE; // Preserve desktop clicks without raising the wallpaper.
        return true;
    }
    return false;
}
