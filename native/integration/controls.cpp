// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <cwchar>

namespace {
HINSTANCE module;
thread_local bool installing = false;
thread_local bool fixture = false;
thread_local DWORD lastRead = 0;
struct Palette {
    COLORREF background, text, button, border, accent;
    bool enabled;
};
thread_local Palette colors { RGB(239, 240, 241), RGB(35, 38, 41), RGB(252, 252, 252), RGB(166, 174, 181),
    RGB(61, 174, 233), true };
thread_local HBRUSH lightBrush = nullptr, darkBrush = nullptr;
UINT restoreMessage()
{
    static UINT message = RegisterWindowMessageW(L"KDE-on-Win.Controls.Restore");
    return message;
}
void refresh(bool force = false)
{
    if (fixture || GetTickCount() - lastRead < (force ? 20u : 1000u))
        return;
    lastRead = GetTickCount();
    // Avoid a 64 KiB stack frame on every hooked window message.
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!length || length > 32700)
        return;
    auto* path = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (length + 64) * sizeof(wchar_t)));
    if (!path)
        return;
    GetEnvironmentVariableW(L"LOCALAPPDATA", path, length);
    wcscat_s(path, length + 64, L"\\plasma-win-session\\appearance.ini");
    wchar_t value[32] {};
    GetPrivateProfileStringW(L"General", L"dark", L"false", value, 32, path);
    bool dark = _wcsicmp(value, L"true") == 0;
    GetPrivateProfileStringW(L"General", L"nativeControls", L"true", value, 32, path);
    colors.enabled = _wcsicmp(value, L"false") != 0;
    colors.background = dark ? RGB(35, 38, 41) : RGB(239, 240, 241);
    colors.text = dark ? RGB(239, 240, 241) : RGB(35, 38, 41);
    colors.button = dark ? RGB(49, 54, 59) : RGB(252, 252, 252);
    colors.border = dark ? RGB(103, 111, 119) : RGB(166, 174, 181);
    GetPrivateProfileStringW(L"General", L"accentColor", L"#3daee9", value, 32, path);
    if (value[0] == L'#' && wcslen(value) == 7) {
        auto rgb = wcstoul(value + 1, nullptr, 16);
        colors.accent = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    }
    HeapFree(GetProcessHeap(), 0, path);
}
HBRUSH backgroundBrush()
{
    auto& brush = colors.background == RGB(35, 38, 41) ? darkBrush : lightBrush;
    if (!brush)
        brush = CreateSolidBrush(colors.background);
    return brush;
}
void releaseBrushes()
{
    if (lightBrush)
        DeleteObject(lightBrush);
    if (darkBrush)
        DeleteObject(darkBrush);
    lightBrush = darkBrush = nullptr;
}

std::wstring className(HWND window)
{
    wchar_t name[128] {};
    GetClassNameW(window, name, 128);
    return name;
}
bool excluded(HWND window)
{
    auto name = className(window);
    return name.rfind(L"Qt", 0) == 0 || name.rfind(L"Chrome_", 0) == 0 || name.rfind(L"Windows.UI", 0) == 0
        || name.rfind(L"Xaml", 0) == 0;
}
thread_local int liveStates = 0;
struct State {
    int references = 1;
    State() { ++liveStates; }
    ~State()
    {
        // Release on the UI thread after its final callback, not in DLL detach.
        if (!--liveStates)
            releaseBrushes();
    }
    void release()
    {
        if (!--references)
            delete this;
    }
    bool button = false, dialog = false, hot = false, restoring = false;
    COLORREF back = CLR_NONE, text = CLR_NONE, textBack = CLR_NONE;
    int list = 0;
};
struct StateGuard {
    State* state;
    explicit StateGuard(State* value)
        : state(value)
    {
        ++state->references;
    }
    ~StateGuard() { state->release(); }
};
void restore(HWND window, State* state)
{
    state->restoring = true;
    if (state->list == 1) {
        SendMessageW(window, LVM_SETBKCOLOR, 0, state->back);
        SendMessageW(window, LVM_SETTEXTCOLOR, 0, state->text);
        SendMessageW(window, LVM_SETTEXTBKCOLOR, 0, state->textBack);
    }
    if (state->list == 2) {
        SendMessageW(window, TVM_SETBKCOLOR, 0, state->back);
        SendMessageW(window, TVM_SETTEXTCOLOR, 0, state->text);
    }
}
void paintButton(HWND window, HDC dc, State* state)
{
    RECT rect {};
    GetClientRect(window, &rect);
    HBRUSH base = CreateSolidBrush(colors.background);
    FillRect(dc, &rect, base);
    DeleteObject(base);
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    int type = style & BS_TYPEMASK;
    bool check = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
    bool radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
    bool group = type == BS_GROUPBOX;
    bool enabled = IsWindowEnabled(window) != FALSE;
    LRESULT status = SendMessageW(window, BM_GETSTATE, 0, 0);
    if (state->restoring)
        return;
    LRESULT checked = SendMessageW(window, BM_GETCHECK, 0, 0);
    if (state->restoring)
        return;
    int unit = MulDiv(16, GetDpiForWindow(window), 96), pad = MulDiv(6, GetDpiForWindow(window), 96);
    HPEN pen = CreatePen(PS_SOLID, (status & BST_FOCUS) ? 2 : 1,
        (state->hot || (status & BST_FOCUS) || type == BS_DEFPUSHBUTTON) ? colors.accent : colors.border);
    auto oldPen = SelectObject(dc, pen);
    HBRUSH brush = CreateSolidBrush((status & BST_PUSHED) ? colors.background : colors.button);
    auto oldBrush = SelectObject(dc, brush);
    RECT text = rect;
    if (check || radio) {
        int top = (rect.bottom - unit) / 2;
        if (radio)
            Ellipse(dc, pad, top, pad + unit, top + unit);
        else
            RoundRect(dc, pad, top, pad + unit, top + unit, 4, 4);
        if (checked != BST_UNCHECKED) {
            HBRUSH mark = CreateSolidBrush(colors.accent);
            auto previous = SelectObject(dc, mark);
            HPEN markPen = CreatePen(PS_SOLID, 2, colors.text);
            auto prior = SelectObject(dc, markPen);
            if (radio)
                Ellipse(dc, pad + 4, top + 4, pad + unit - 4, top + unit - 4);
            else if (checked == BST_INDETERMINATE) {
                RECT dash { pad + 4, top + unit / 2 - 1, pad + unit - 4, top + unit / 2 + 2 };
                FillRect(dc, &dash, mark);
            } else {
                MoveToEx(dc, pad + 3, top + unit / 2, nullptr);
                LineTo(dc, pad + unit / 2 - 1, top + unit - 4);
                LineTo(dc, pad + unit - 3, top + 4);
            }
            SelectObject(dc, prior);
            DeleteObject(markPen);
            SelectObject(dc, previous);
            DeleteObject(mark);
        }
        text.left += unit + 2 * pad;
    } else if (group) {
        RECT frame = rect;
        frame.top += pad;
        RoundRect(dc, frame.left, frame.top, frame.right - 1, frame.bottom - 1, 4, 4);
        text.left += 2 * pad;
        text.bottom = 2 * pad + 4;
    } else
        RoundRect(dc, rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1, 6, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? colors.text : colors.border);
    auto font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
    if (state->restoring)
        return;
    auto oldFont = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    wchar_t label[2048] {};
    GetWindowTextW(window, label, 2048);
    UINT flags = (check || radio || group) ? DT_LEFT : DT_CENTER;
    flags |= DT_VCENTER | DT_SINGLELINE;
    if (style & BS_MULTILINE) {
        flags &= ~DT_SINGLELINE;
        flags |= DT_WORDBREAK;
    }
    if (SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL)
        flags |= DT_HIDEPREFIX;
    if (!state->restoring)
        DrawTextW(dc, label, -1, &text, flags);
    SelectObject(dc, oldFont);
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data)
{
    auto* state = reinterpret_cast<State*>(data);
    StateGuard guard(state);
    if (state->restoring)
        return DefSubclassProc(window, message, wp, lp);
    if (message == restoreMessage() || message == WM_NCDESTROY) {
        state->restoring = true;
        if (message == WM_NCDESTROY)
            SetPropW(window, L"KDE-on-Win.Destroying", reinterpret_cast<HANDLE>(1));
        if (message != WM_NCDESTROY) {
            EnumChildWindows(
                window,
                [](HWND child, LPARAM) -> BOOL {
                    SendMessageW(child, restoreMessage(), 0, 0);
                    return TRUE;
                },
                0);
            restore(window, state);
        }
        RemoveWindowSubclass(window, procedure, 0x4b4445);
        state->release();
        InvalidateRect(window, nullptr, TRUE);
        return DefSubclassProc(window, message, wp, lp);
    }
    refresh(message == WM_THEMECHANGED);
    if (!colors.enabled) {
        state->restoring = true;
        if (!(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD))
            EnumChildWindows(
                window,
                [](HWND child, LPARAM) -> BOOL {
                    PostMessageW(child, WM_THEMECHANGED, 0, 0);
                    return TRUE;
                },
                0);
        restore(window, state);
        RemoveWindowSubclass(window, procedure, 0x4b4445);
        state->release();
        InvalidateRect(window, nullptr, TRUE);
        return DefSubclassProc(window, message, wp, lp);
    }
    if (message >= WM_CTLCOLORMSGBOX && message <= WM_CTLCOLORSTATIC) {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetTextColor(dc, colors.text);
        SetBkColor(dc, colors.background);
        return reinterpret_cast<LRESULT>(backgroundBrush());
    }
    if (state->dialog && message == WM_ERASEBKGND) {
        RECT rect {};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wp), &rect, backgroundBrush());
        return 1;
    }
    if (state->button && message == WM_MOUSEMOVE) {
        state->hot = true;
        TRACKMOUSEEVENT track { sizeof(track), TME_LEAVE, window, 0 };
        TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (state->button && message == WM_MOUSELEAVE) {
        state->hot = false;
        InvalidateRect(window, nullptr, FALSE);
    }
    if (state->button && (message == WM_PAINT || message == WM_PRINTCLIENT)) {
        const auto style = GetWindowLongPtrW(window, GWL_STYLE);
        const auto type = style & BS_TYPEMASK;
        if (type == BS_OWNERDRAW || type == BS_USERBUTTON || (style & (BS_BITMAP | BS_ICON)))
            return DefSubclassProc(window, message, wp, lp);
        PAINTSTRUCT paint {};
        HDC dc = message == WM_PAINT ? BeginPaint(window, &paint) : reinterpret_cast<HDC>(wp);
        paintButton(window, dc, state);
        if (message == WM_PAINT)
            EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_THEMECHANGED) {
        if (!(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD))
            EnumChildWindows(
                window,
                [](HWND child, LPARAM) -> BOOL {
                    PostMessageW(child, WM_THEMECHANGED, 0, 0);
                    return TRUE;
                },
                0);
        if (state->list == 1) {
            SendMessageW(window, LVM_SETBKCOLOR, 0, colors.background);
            SendMessageW(window, LVM_SETTEXTCOLOR, 0, colors.text);
            SendMessageW(window, LVM_SETTEXTBKCOLOR, 0, colors.background);
        }
        if (state->list == 2) {
            SendMessageW(window, TVM_SETBKCOLOR, 0, colors.background);
            SendMessageW(window, TVM_SETTEXTCOLOR, 0, colors.text);
        }
        InvalidateRect(window, nullptr, TRUE);
    }
    return DefSubclassProc(window, message, wp, lp);
}
void attach(HWND window, bool parentOnly = false)
{
    if (!IsWindow(window) || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId())
        return;
    DWORD_PTR previous = 0;
    if (GetWindowSubclass(window, procedure, 0x4b4445, &previous) || excluded(window))
        return;
    if (GetPropW(window, L"KDE-on-Win.Destroying"))
        return;
    auto name = className(window);
    auto style = GetWindowLongPtrW(window, GWL_STYLE);
    int type = style & BS_TYPEMASK;
    bool button = name == L"Button" && type != BS_OWNERDRAW && type != BS_USERBUTTON && !(style & (BS_BITMAP | BS_ICON))
        && type <= BS_AUTORADIOBUTTON;
    bool dialog = name == L"#32770", list = name == WC_LISTVIEWW, tree = name == WC_TREEVIEWW;
    bool child = button || name == L"Edit" || name == L"Static" || name == L"ListBox" || list || tree;
    if (!parentOnly && !child && !dialog)
        return;
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&attach), &pinned);
    auto* state = new State;
    state->button = button;
    state->dialog = dialog;
    if (list) {
        state->list = 1;
        state->back = static_cast<COLORREF>(SendMessageW(window, LVM_GETBKCOLOR, 0, 0));
        state->text = static_cast<COLORREF>(SendMessageW(window, LVM_GETTEXTCOLOR, 0, 0));
        state->textBack = static_cast<COLORREF>(SendMessageW(window, LVM_GETTEXTBKCOLOR, 0, 0));
    }
    if (tree) {
        state->list = 2;
        state->back = static_cast<COLORREF>(SendMessageW(window, TVM_GETBKCOLOR, 0, 0));
        state->text = static_cast<COLORREF>(SendMessageW(window, TVM_GETTEXTCOLOR, 0, 0));
    }
    if (!SetWindowSubclass(window, procedure, 0x4b4445, reinterpret_cast<DWORD_PTR>(state))) {
        delete state;
        return;
    }
    if (child && GetParent(window))
        attach(GetParent(window), true);
    PostMessageW(window, WM_THEMECHANGED, 0, 0);
}
}
extern "C" LRESULT CALLBACK KDEControlHook(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && !installing) {
        installing = true;
        try {
            auto* message = reinterpret_cast<CWPSTRUCT*>(lp);
            refresh();
            if (colors.enabled)
                attach(message->hwnd);
        } catch (...) {
        }
        installing = false;
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}
extern "C" BOOL __stdcall KDEControlSelfTest()
{
    // Exercise the real profile reader against an isolated, disposable profile.
    wchar_t temporary[32768] {}, root[32768] {};
    GetTempPathW(32768, temporary);
    swprintf_s(root, L"%sKDE-style-fixture-%lu-%llu", temporary, GetCurrentProcessId(), GetTickCount64());
    if (!CreateDirectoryW(root, nullptr))
        return FALSE;
    const auto directory = std::wstring(root) + L"\\plasma-win-session";
    CreateDirectoryW(directory.c_str(), nullptr);
    const auto profile = directory + L"\\appearance.ini";
    WritePrivateProfileStringW(L"General", L"dark", L"true", profile.c_str());
    WritePrivateProfileStringW(L"General", L"nativeControls", L"false", profile.c_str());
    WritePrivateProfileStringW(L"General", L"accentColor", L"#aa1122", profile.c_str());
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    std::wstring original(length ? length : 1, L'\0');
    GetEnvironmentVariableW(L"LOCALAPPDATA", original.data(), static_cast<DWORD>(original.size()));
    SetEnvironmentVariableW(L"LOCALAPPDATA", root);
    lastRead = GetTickCount() - 2000;
    refresh(true);
    const bool parsed = !colors.enabled && colors.background == RGB(35, 38, 41) && colors.accent == RGB(170, 17, 34);
    SetEnvironmentVariableW(L"LOCALAPPDATA", length ? original.c_str() : nullptr);
    DeleteFileW(profile.c_str());
    RemoveDirectoryW(directory.c_str());
    RemoveDirectoryW(root);
    fixture = true;
    colors.enabled = true;
    HWND parent = CreateWindowExW(
        0, L"STATIC", L"Hidden style fixture", WS_OVERLAPPEDWINDOW, 0, 0, 240, 140, nullptr, nullptr, module, nullptr);
    HWND button = CreateWindowExW(
        0, L"BUTTON", L"KDE button", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 200, 40, parent, nullptr, module, nullptr);
    attach(button);
    SendMessageW(button, BM_SETCHECK, BST_CHECKED, 0);
    HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 200, 40);
    auto old = SelectObject(dc, bitmap);
    SendMessageW(button, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    bool ok = parsed && GetPixel(dc, 190, 20) == colors.background
        && SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED && !IsWindowVisible(parent);
    int customDraws = 0;
    SUBCLASSPROC custom = [](HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) -> LRESULT {
        if (message == WM_DRAWITEM) {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
            auto brush = CreateSolidBrush(RGB(0, 255, 0));
            FillRect(item->hDC, &item->rcItem, brush);
            DeleteObject(brush);
            ++*reinterpret_cast<int*>(data);
            return TRUE;
        }
        return DefSubclassProc(window, message, wp, lp);
    };
    SetWindowSubclass(parent, custom, 0x1234, reinterpret_cast<DWORD_PTR>(&customDraws));
    SendMessageW(button, BM_SETSTYLE, BS_OWNERDRAW, FALSE);
    SendMessageW(button, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    ok = ok && customDraws > 0 && GetPixel(dc, 190, 20) == RGB(0, 255, 0);
    RemoveWindowSubclass(parent, custom, 0x1234);
    const int baseline = liveStates;
    HWND doomed = CreateWindowExW(
        0, L"BUTTON", L"Reentrant destruction", WS_CHILD, 0, 0, 200, 40, parent, nullptr, module, nullptr);
    attach(doomed);
    struct DestructionProbe {
        int expected;
        bool retained = false;
    } probe { liveStates };
    SUBCLASSPROC destroyDuringPaint
        = [](HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) -> LRESULT {
        if (message == BM_GETSTATE) {
            auto* probe = reinterpret_cast<DestructionProbe*>(data);
            DestroyWindow(window);
            probe->retained = liveStates == probe->expected;
            return 0;
        }
        return DefSubclassProc(window, message, wp, lp);
    };
    SetWindowSubclass(doomed, destroyDuringPaint, 0x5678, reinterpret_cast<DWORD_PTR>(&probe));
    SendMessageW(doomed, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    ok = ok && probe.retained && !IsWindow(doomed) && liveStates == baseline;
    const auto originalBackground = colors.background;
    colors.background = RGB(239, 240, 241);
    const HBRUSH light = backgroundBrush();
    colors.background = RGB(35, 38, 41);
    const HBRUSH dark = backgroundBrush();
    colors.background = originalBackground;
    ok &= light && dark;
    SendMessageW(parent, restoreMessage(), 0, 0);
    ok &= liveStates == 0 && !lightBrush && !darkBrush;
    DWORD_PTR value = 0;
    ok = ok && !GetWindowSubclass(button, procedure, 0x4b4445, &value);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    DestroyWindow(parent);
    return ok;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance;
    }
    return TRUE;
}
