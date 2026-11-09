// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QTableWidget>
#include <QToolButton>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <QFontDatabase>
#define NOMINMAX
#include <windows.h>
int main(int argc, char** argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains(QStringLiteral("--native-frame-self-test"))) {
        QMainWindow hidden;
        hidden.ensurePolished();
        hidden.winId();
        auto pump = [&] {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 100) {
                app.processEvents();
                QThread::msleep(2);
            }
        };
        pump();
        HWND handle = reinterpret_cast<HWND>(hidden.winId());
        bool ok = IsWindow(handle) && !IsWindowVisible(handle) && hidden.findChild<QWidget*>("PlasmaBreezeTitleBar");
        SetWindowLongPtrW(handle, GWL_STYLE, GetWindowLongPtrW(handle, GWL_STYLE) | WS_CAPTION);
        pump();
        ok &= (GetWindowLongPtrW(handle, GWL_STYLE) & WS_CAPTION) == 0;
        hidden.setWindowFlag(Qt::WindowStaysOnTopHint, true);
        pump();
        handle = reinterpret_cast<HWND>(hidden.winId());
        pump();
        ok &= IsWindow(handle) && !IsWindowVisible(handle) && (GetWindowLongPtrW(handle, GWL_STYLE) & WS_CAPTION) == 0;
        // Reproduce a native maximization while Qt still reports a normal
        // child widget. The hidden ancestor keeps this test off the desktop.
        QWidget holder;
        holder.winId();
        QMainWindow child;
        child.setParent(&holder, Qt::Widget);
        child.ensurePolished();
        child.winId();
        pump();
        const HWND embedded = reinterpret_cast<HWND>(child.winId());
        const bool childWindow = GetWindowLongPtrW(embedded, GWL_STYLE) & WS_CHILD;
        ok &= childWindow;
        if (childWindow) {
            ShowWindow(embedded, SW_MAXIMIZE);
            pump();
            auto* bar = child.findChild<QWidget*>("PlasmaBreezeTitleBar");
            const auto buttons = bar ? bar->findChildren<QToolButton*>() : QList<QToolButton*>();
            ok &= IsZoomed(embedded) && buttons.size() == 3;
            if (buttons.size() == 3)
                buttons[1]->click();
            pump();
            ok &= !IsZoomed(embedded) && !IsWindowVisible(embedded)
                && !IsWindowVisible(reinterpret_cast<HWND>(holder.winId()));
        }
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": native caption style repair and HWND recreation on a hidden test-owned window\n";
        return ok ? 0 : 1;
    }
    const int font = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
    if (font >= 0)
        app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).value(0), 10));
    QMainWindow window;
    window.setWindowTitle("Breeze window integration");
    window.resize(640, 400);
    auto* menu = window.menuBar()->addMenu("File");
    menu->addAction("New Window");
    menu->addAction("Close");
    auto* table = new QTableWidget(3, 3);
    table->setHorizontalHeaderLabels({ "Process", "CPU", "Memory" });
    table->setItem(0, 0, new QTableWidgetItem("Konsole"));
    table->setItem(1, 0, new QTableWidgetItem("Dolphin"));
    table->setItem(2, 0, new QTableWidgetItem("Process Monitor"));
    window.setCentralWidget(table);
    window.show();
    auto pump = [&] {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 150) {
            app.processEvents();
            QThread::msleep(5);
        }
    };
    pump();
    QWidget* bar = window.findChild<QWidget*>("PlasmaBreezeTitleBar");
    bool ok = bar && window.windowFlags().testFlag(Qt::FramelessWindowHint) && window.contentsMargins().top() >= 32
        && window.menuBar()->actions().size() == 1;
    if (bar) {
        const auto buttons = bar->findChildren<QToolButton*>();
        ok &= buttons.size() == 3;
        window.grab().save("breeze-window-preview.png");
        window.showFullScreen();
        pump();
        ok &= bar->isHidden() && window.contentsMargins().top() == 0;
        window.showNormal();
        pump();
        ok &= !bar->isHidden() && window.contentsMargins().top() >= 32;
        if (buttons.size() == 3) {
            buttons[1]->click();
            pump();
            ok &= window.isMaximized();
            buttons[1]->click();
            pump();
            ok &= !window.isMaximized();
            buttons[0]->click();
            pump();
            ok &= window.isMinimized();
            window.showNormal();
            pump();
            buttons[2]->click();
            pump();
            ok &= !window.isVisible();
        }
    }
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": Breeze chrome, content margins, preserved menu, maximize/restore/minimize/close\n";
    return ok ? 0 : 1;
}
