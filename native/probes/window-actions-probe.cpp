// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QWindow>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include "windowswindowactions.h"
#include "windowsfullscreenpolicy.h"
#include "../apps/desktop-toggle.h"
static void pump()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 100) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QWindow holder;
    holder.create();
    HWND window = CreateWindowExW(0, L"STATIC", L"Owned fixture",
        WS_CHILD | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME, 0, 0, 300, 200,
        reinterpret_cast<HWND>(holder.winId()), nullptr, GetModuleHandleW(nullptr), nullptr);
    const RECT monitor { -1920, 0, 0, 1080 }, fullscreen { -1920, 0, 0, 1080 }, normal { -1800, 10, -10, 1040 };
    const bool geometry = windowsCoversMonitor(fullscreen, monitor, false)
        && !windowsCoversMonitor(normal, monitor, false) && !windowsCoversMonitor(fullscreen, monitor, true);
    const HWND holderWindow = reinterpret_cast<HWND>(holder.winId());
    const HWND dialog = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Hidden owned dialog", WS_OVERLAPPEDWINDOW, 0, 0,
        80, 60, holderWindow, nullptr, GetModuleHandleW(nullptr), nullptr);
    const bool activation = dialog
        && TaskManager::activationTask(dialog, [=](HWND candidate) { return candidate == holderWindow; })
            == holderWindow
        && TaskManager::activationTask(dialog, [=](HWND candidate) { return candidate == dialog; }) == dialog
        && !TaskManager::activationTask(dialog, [](HWND) { return false; });
    if (dialog)
        DestroyWindow(dialog);
    const DWORD pid = GetCurrentProcessId();
    bool ok = activation && geometry && ordinaryDesktopWindowStyle(WS_SYSMENU | WS_THICKFRAME, 0)
        && !ordinaryDesktopWindowStyle(WS_POPUP, 0) && !ordinaryDesktopWindowStyle(WS_SYSMENU, WS_EX_TOOLWINDOW)
        && window && !IsWindowVisible(reinterpret_cast<HWND>(holder.winId()));
    ok &= TaskManager::windowCommandAllowed(window, pid, SC_MAXIMIZE)
        && !TaskManager::requestWindowCommand(window, pid + 1, SC_CLOSE);
    EnableMenuItem(GetSystemMenu(window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    ok &= !TaskManager::requestWindowCommand(window, pid, SC_CLOSE);
    EnableMenuItem(GetSystemMenu(window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
    ok &= TaskManager::requestWindowCommand(window, pid, SC_MINIMIZE);
    pump();
    ok &= IsIconic(window);
    ok &= TaskManager::requestWindowCommand(window, pid, SC_RESTORE);
    pump();
    ok &= !IsIconic(window);
    ok &= TaskManager::requestWindowCommand(window, pid, SC_MAXIMIZE);
    pump();
    ok &= IsZoomed(window);
    TaskManager::requestWindowCommand(window, pid, SC_RESTORE);
    pump();
    {
        SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) & ~WS_CAPTION);
        DesktopToggle toggle;
        ok &= toggle.minimize(window);
        toggle.restore();
        pump();
        ok &= !IsIconic(window);
    }
    ok &= TaskManager::requestWindowCommand(window, pid, SC_CLOSE);
    pump();
    ok &= !IsWindow(window);
    ok &= !TaskManager::requestWindowCommand(window, pid, SC_CLOSE)
        && !IsWindowVisible(reinterpret_cast<HWND>(holder.winId()));
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": owned hidden-window minimize/maximize/restore/close, disabled close, wrong identity and "
                           "desktop toggle cleanup\n";
    return ok ? 0 : 1;
}
