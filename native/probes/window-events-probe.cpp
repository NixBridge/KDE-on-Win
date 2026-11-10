// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include "windowslist.h"
#include <kwindowsystemwindows.h>
#include <QPointer>
#include <thread>
static void pump()
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 180) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
}
int main(int argc, char** argv)
{
    bool ok = true;
    QPointer<KWindowSystemWindowsPrivate> backend;
    {
        QGuiApplication app(argc, argv);
        auto* facade = KWindowSystemWindows::self();
        ok &= facade && facade->parent() == &app;
        backend = app.findChild<KWindowSystemWindowsPrivate*>();
        ok &= backend && backend->parent() == &app;
        bool workerRejected = false;
        std::thread worker([&] {
            workerRejected = KWindowSystemWindows::windows().isEmpty() && KWindowSystemWindows::self() == nullptr;
        });
        worker.join();
        ok &= workerRejected;
        for (int pass = 0; pass < 3; ++pass) {
            WindowList list(nullptr, GetCurrentProcessId()); // WinEvent hooks only monitor this test process.
            int added = 0, changed = 0, removed = 0, geometry = 0;
            HWND fixture = nullptr;
            QObject::connect(&list, &KWindowSystemWindowsPrivate::windowAdded, [&](WId id) {
                if (id == reinterpret_cast<WId>(fixture))
                    ++added;
            });
            QObject::connect(&list, &KWindowSystemWindowsPrivate::windowChanged, [&](WId id, bool geometryOnly) {
                if (id == reinterpret_cast<WId>(fixture)) {
                    ++changed;
                    if (geometryOnly)
                        ++geometry;
                }
            });
            QObject::connect(&list, &KWindowSystemWindowsPrivate::windowRemoved, [&](WId id) {
                if (id == reinterpret_cast<WId>(fixture))
                    ++removed;
            });
            fixture = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_APPWINDOW, L"STATIC", L"Private fixture",
                WS_OVERLAPPEDWINDOW, -32000, -32000, 80, 60, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            ShowWindow(fixture, SW_SHOWNOACTIVATE);
            pump();
            ok &= added > 0;
            const int before = changed;
            SetWindowTextW(fixture, L"Updated fixture");
            NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            ok &= changed > before;
            SetWindowPos(fixture, nullptr, -31998, -32000, 80, 60, SWP_NOACTIVATE | SWP_NOZORDER);
            NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            ok &= geometry > 0;
            HWND owner = CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"Hidden owner", WS_OVERLAPPEDWINDOW, -32000,
                -32000, 60, 40, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            SetWindowLongPtrW(fixture, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
            const auto beforeOwned = changed;
            NotifyWinEvent(EVENT_OBJECT_STATECHANGE, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            ok &= owner && changed > beforeOwned;
            const auto priorExcluded = removed;
            SetWindowLongPtrW(fixture, GWL_EXSTYLE, WS_EX_NOACTIVATE);
            NotifyWinEvent(EVENT_OBJECT_STATECHANGE, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            ok &= removed > priorExcluded;
            SetWindowLongPtrW(fixture, GWL_EXSTYLE, WS_EX_NOACTIVATE | WS_EX_APPWINDOW);
            NotifyWinEvent(EVENT_OBJECT_STATECHANGE, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            const auto priorMinimize = changed;
            ShowWindow(fixture, SW_SHOWMINNOACTIVE);
            NotifyWinEvent(EVENT_SYSTEM_MINIMIZESTART, fixture, OBJID_WINDOW, CHILDID_SELF);
            pump();
            ok &= changed > priorMinimize;
            ShowWindow(fixture, SW_SHOWNOACTIVATE);
            ShowWindow(fixture, SW_HIDE);
            pump();
            ok &= removed > 0;
            ShowWindow(fixture, SW_SHOWNOACTIVATE);
            pump();
            const auto prior = removed;
            DestroyWindow(fixture);
            pump();
            ok &= removed > prior;
            DestroyWindow(owner);
        }
        pump();
    }
    ok &= backend.isNull() && KWindowSystemWindows::windows().isEmpty() && KWindowSystemWindows::self() == nullptr;
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": process-scoped WinEvents, own off-screen HWND create/name/hide/show/destroy and repeated "
                           "hook teardown; actual backend parent/GUI-thread/after-application lifetime\n";
    return ok ? 0 : 1;
}
