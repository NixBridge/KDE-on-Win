// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <string>
static HHOOK hook = nullptr;
static FILETIME lastWrite {};
static std::wstring settingsPath;
static UINT restoreMessage() { return RegisterWindowMessageW(L"KDE-on-Win.Controls.Restore"); }
static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    if (message == WM_TIMER) {
        WIN32_FILE_ATTRIBUTE_DATA info {};
        if (GetFileAttributesExW(settingsPath.c_str(), GetFileExInfoStandard, &info)
            && CompareFileTime(&info.ftLastWriteTime, &lastWrite) != 0) {
            lastWrite = info.ftLastWriteTime;
            PostMessageW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0);
        }
        return 0;
    }
    if (message == WM_CLOSE) {
        if (hook) {
            UnhookWindowsHookEx(hook);
            hook = nullptr;
        }
        PostMessageW(HWND_BROADCAST, restoreMessage(), 0, 0);
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int)
{
    wchar_t path[32768] {};
    GetModuleFileNameW(nullptr, path, 32768);
    auto* last = wcsrchr(path, L'\\');
    if (!last)
        return 2;
    wcscpy_s(last + 1, 32768 - (last + 1 - path), L"kde-controls.dll");
    HMODULE library = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library)
        return 3;
    if (wcsstr(args, L"--self-test")) {
        auto test = reinterpret_cast<BOOL(__stdcall*)()>(GetProcAddress(library, "KDEControlSelfTest"));
        bool ok = test && test();
        std::puts(ok
                ? "PASS: hidden native control paint, checkbox state and subclass restoration; no global hook installed"
                : "FAIL: control style fixture");
        return ok ? 0 : 1;
    }
    const wchar_t* name = sizeof(void*) == 8 ? L"KDEStyleHost64" : L"KDEStyleHost32";
    HWND existing = FindWindowW(name, nullptr);
    if (wcsstr(args, L"--stop")) {
        if (existing)
            PostMessageW(existing, WM_CLOSE, 0, 0);
        else
            // Recover pinned window callbacks even if their host was terminated.
            PostMessageW(HWND_BROADCAST, restoreMessage(), 0, 0);
        return 0;
    }
    if (existing)
        return 0;
    WNDCLASSW klass {};
    klass.hInstance = instance;
    klass.lpszClassName = name;
    klass.lpfnWndProc = procedure;
    RegisterClassW(&klass);
    HWND window = CreateWindowExW(
        WS_EX_TOOLWINDOW, name, L"KDE control style", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    auto callback = reinterpret_cast<HOOKPROC>(GetProcAddress(library, "KDEControlHook"));
    hook = callback ? SetWindowsHookExW(WH_CALLWNDPROC, callback, library, 0) : nullptr;
    if (!window || !hook) {
        if (window)
            DestroyWindow(window);
        return 4;
    }
    wchar_t local[32768] {};
    GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    settingsPath = std::wstring(local) + L"\\plasma-win-session\\appearance.ini";
    SetTimer(window, 1, 1000, nullptr);
    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
