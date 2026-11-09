// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QWindow>
#include <QTextStream>
#include "windowsdesktopstacking.h"

class DesktopFixture : public QWindow {
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override
    {
        const auto msg = static_cast<MSG*>(message);
        if (constrainWindowsDesktopStacking(msg->message, msg->lParam, result))
            return true;
        return QWindow::nativeEvent(type, message, result);
    }
};

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QWindow parent;
    parent.create(); // Never show any fixture or touch the user's windows.
    DesktopFixture desktop;
    desktop.setParent(&parent);
    desktop.create();
    QWindow application;
    application.setParent(&parent);
    application.create();
    const auto handle = reinterpret_cast<HWND>(desktop.winId());
    const auto other = reinterpret_cast<HWND>(application.winId());
    bool ok = true;
    for (HWND request : { HWND_TOP, HWND_TOPMOST, HWND_NOTOPMOST, other }) {
        SetWindowPos(handle, request, 0, 0, 100, 100, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ok &= GetWindow(handle, GW_HWNDNEXT) == nullptr;
        ok &= !(GetWindowLongPtrW(handle, GWL_EXSTYLE) & WS_EX_TOPMOST);
        ok &= !IsIconic(other) && !IsWindowVisible(other);
    }
    SetWindowPos(handle, HWND_TOP, 0, 0, 110, 110, SWP_NOZORDER | SWP_NOACTIVATE);
    ok &= GetWindow(handle, GW_HWNDNEXT) == nullptr;
    ok &= SendMessageW(handle, WM_MOUSEACTIVATE, 0, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN)) == MA_NOACTIVATE;
    ok &= !IsWindowVisible(reinterpret_cast<HWND>(parent.winId()));
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": native desktop raise/topmost/move/activation requests constrained on hidden fixtures\n";
    return ok ? 0 : 1;
}
