// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
int wmain()
{
    wchar_t path[32768] {};
    GetModuleFileNameW(nullptr, path, 32768);
    auto* last = wcsrchr(path, L'\\');
    if (!last)
        return 2;
    wcscpy_s(last + 1, 32768 - (last + 1 - path), L"kde-logon-provider.dll");
    HMODULE module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
        return 3;
    auto test = reinterpret_cast<HRESULT(__stdcall*)()>(GetProcAddress(module, "KDELogonSelfTest"));
    bool ok = test && SUCCEEDED(test());
    FreeLibrary(module);
    std::puts(ok ? "PASS: synthetic credential protection/serialization, user-tile fallback and unsupported scenario "
                   "rejection; no account authentication or registration"
                 : "FAIL: credential provider self-test");
    return ok ? 0 : 1;
}
