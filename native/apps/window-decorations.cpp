// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>
#include <QMouseEvent>
#include <QTimer>
#include <QMenu>
#include <QTextStream>
#include <QContextMenuEvent>
#include <QSet>
#include <QImage>
#include <QPixmap>
#include <QAbstractNativeEventFilter>
#include <QDBusConnection>
#include <KIconTheme>
#include <KIconLoader>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <oleacc.h>
#include <vector>
#include "appearance.h"

namespace {
bool sameUserAndIntegrity(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    HANDLE remote = nullptr, local = nullptr;
    const bool opened = OpenProcessToken(process, TOKEN_QUERY, &remote) && OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &local);
    auto information = [](HANDLE token, TOKEN_INFORMATION_CLASS type) {
        DWORD size = 0; GetTokenInformation(token, type, nullptr, 0, &size);
        std::vector<BYTE> data(size);
        if (!size || !GetTokenInformation(token, type, data.data(), size, &size)) data.clear();
        return data;
    };
    bool permitted = false;
    if (opened) {
        const auto user = information(remote, TokenUser), own = information(local, TokenUser);
        const auto level = information(remote, TokenIntegrityLevel), ownLevel = information(local, TokenIntegrityLevel);
        if (!user.empty() && !own.empty() && !level.empty() && !ownLevel.empty()) {
            const auto sid = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(level.data())->Label.Sid;
            const auto localSid = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(ownLevel.data())->Label.Sid;
            const DWORD integrity = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid)-1);
            const DWORD localIntegrity = *GetSidSubAuthority(localSid, *GetSidSubAuthorityCount(localSid)-1);
            permitted = EqualSid(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid, reinterpret_cast<const TOKEN_USER*>(own.data())->User.Sid) && integrity <= localIntegrity;
        }
    }
    if (remote) CloseHandle(remote); if (local) CloseHandle(local); CloseHandle(process); return permitted;
}
bool standardCaption(HWND window, bool fixture = false)
{
    if (!IsWindow(window) || (!fixture && (!IsWindowVisible(window) || IsIconic(window)))) return false;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    if ((style & WS_CAPTION) != WS_CAPTION || (style & WS_CHILD) || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) return false;
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    if (!fixture && pid == GetCurrentProcessId()) return false;
    if (!sameUserAndIntegrity(pid)) return false;
    DWORD cloaked = 0; DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)); if (cloaked) return false;
    RECT rect{}; GetWindowRect(window, &rect);
    POINT client{}; ClientToScreen(window, &client);
    const UINT dpi = GetDpiForWindow(window);
    const int caption = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
    // A client extended into the native caption is self-decorated, even if
    // it retains WS_CAPTION for DWM shadows and resizing.
    if (client.y - rect.top < caption) return false;
    MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)
        && rect.left <= monitor.rcMonitor.left && rect.top <= monitor.rcMonitor.top
        && rect.right >= monitor.rcMonitor.right && rect.bottom >= monitor.rcMonitor.bottom && !IsZoomed(window)) return false;
    return true;
}
QRect captionRectangle(HWND target)
{
    TITLEBARINFO info{}; info.cbSize = sizeof(info);
    if (!GetTitleBarInfo(target, &info) || (info.rgstate[0] & STATE_SYSTEM_INVISIBLE)) return {};
    const RECT &rect = info.rcTitleBar;
    return QRect(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);
}
}

class Caption final : public QWidget
{
public:
    explicit Caption(HWND target) : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint), target(target)
    {
        GetWindowThreadProcessId(target, &ownerPid);
        setAttribute(Qt::WA_ShowWithoutActivating); setAutoFillBackground(true);
        auto *layout = new QHBoxLayout(this); layout->setContentsMargins(6, 0, 0, 0); layout->setSpacing(0);
        icon = new QLabel(this); icon->setFixedWidth(24); icon->setAttribute(Qt::WA_TransparentForMouseEvents); layout->addWidget(icon);
        DWORD_PTR nativeIcon = 0;
        SendMessageTimeoutW(target, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 30, &nativeIcon);
        if (!nativeIcon) nativeIcon = GetClassLongPtrW(target, GCLP_HICONSM);
        if (nativeIcon) icon->setPixmap(QPixmap::fromImage(QImage::fromHICON(reinterpret_cast<HICON>(nativeIcon))).scaled(20,20,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        title = new QLabel(this); title->setAlignment(Qt::AlignCenter); title->setMinimumWidth(0); title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); title->setAttribute(Qt::WA_TransparentForMouseEvents); layout->addWidget(title, 1);
        auto button = [&](const QString &name, const QString &tip, UINT command) {
            auto *widget = new QToolButton(this); widget->setAutoRaise(true); widget->setFocusPolicy(Qt::NoFocus); widget->setToolTip(tip); widget->setFixedWidth(36);
            widget->setIcon(QIcon(KIconLoader::global()->loadIcon(name, KIconLoader::Toolbar, 16))); layout->addWidget(widget);
            connect(widget, &QToolButton::clicked, this, [this, command] { act(command); }); return widget;
        };
        minimize = button(QStringLiteral("window-minimize"), tr("Minimize"), SC_MINIMIZE);
        maximize = button(QStringLiteral("window-maximize"), tr("Maximize / Restore"), SC_MAXIMIZE);
        closeButton = button(QStringLiteral("window-close"), tr("Close"), SC_CLOSE);
        SetWindowLongPtrW(reinterpret_cast<HWND>(winId()), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(target));
        oldBorderValid = SUCCEEDED(DwmGetWindowAttribute(target, DWMWA_BORDER_COLOR, &oldBorder, sizeof(oldBorder)));
        updateCaption();
    }
    ~Caption() override { if (oldBorderValid && ownedTarget()) DwmSetWindowAttribute(target, DWMWA_BORDER_COLOR, &oldBorder, sizeof(oldBorder)); }
    void updateCaption()
    {
        if (!ownedTarget() || !standardCaption(target)) { hide(); return; }
        const QRect rect = captionRectangle(target);
        if (rect.isEmpty()) { hide(); return; }
        // Native geometry is in physical pixels. Qt's HWND positioning below
        // avoids mixing coordinate origins on monitors with different scaling.
        wchar_t name[1024]{}; GetWindowTextW(target, name, 1024); title->setText(QString::fromWCharArray(name));
        const bool active = GetForegroundWindow() == target;
        auto colors = qApp->palette(); colors.setCurrentColorGroup(active ? QPalette::Active : QPalette::Inactive); setPalette(colors);
        const auto color = colors.color(QPalette::Window); const COLORREF border = RGB(color.red(),color.green(),color.blue());
        if (oldBorderValid) DwmSetWindowAttribute(target, DWMWA_BORDER_COLOR, &border, sizeof(border));
        const LONG_PTR style = GetWindowLongPtrW(target, GWL_STYLE);
        minimize->setVisible(style & WS_MINIMIZEBOX); maximize->setVisible(style & WS_MAXIMIZEBOX);
        closeButton->setVisible(style & WS_SYSMENU);
        const UINT closeState = GetMenuState(GetSystemMenu(target, FALSE), SC_CLOSE, MF_BYCOMMAND);
        closeButton->setEnabled(closeState != UINT(-1) && !(closeState & (MF_DISABLED | MF_GRAYED)));
        maximize->setIcon(QIcon(KIconLoader::global()->loadIcon(IsZoomed(target) ? QStringLiteral("window-restore") : QStringLiteral("window-maximize"), KIconLoader::Toolbar, 16)));
        if (!isVisible()) show();
        SetWindowPos(reinterpret_cast<HWND>(winId()), nullptr, rect.x(), rect.y(), rect.width(), rect.height(), SWP_NOACTIVATE | SWP_NOZORDER);
    }
    HWND target;
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && ownedTarget()) { POINT cursor{}; GetCursorPos(&cursor); SetForegroundWindow(target); ReleaseCapture(); PostMessageW(target, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(cursor.x, cursor.y)); }
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override { if (event->button() == Qt::LeftButton) act(SC_MAXIMIZE); }
    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu;
        for (const auto command : {SC_RESTORE, SC_MINIMIZE, SC_MAXIMIZE, SC_CLOSE}) {
            const QString label = command == SC_RESTORE ? tr("Restore") : command == SC_MINIMIZE ? tr("Minimize") : command == SC_MAXIMIZE ? tr("Maximize") : tr("Close");
            auto *action = menu.addAction(label);
            const UINT state = GetMenuState(GetSystemMenu(target, FALSE), command, MF_BYCOMMAND);
            action->setEnabled(state != UINT(-1) && !(state & (MF_DISABLED | MF_GRAYED)));
            connect(action, &QAction::triggered, this, [this, command] { act(command); });
        }
        menu.exec(event->globalPos());
    }
private:
    bool ownedTarget() const { DWORD pid = 0; return GetWindowThreadProcessId(target, &pid) && pid == ownerPid; }
    void act(UINT command) {
        if (!ownedTarget()) return;
        if (command == SC_MAXIMIZE && IsZoomed(target)) command = SC_RESTORE;
        const UINT state = GetMenuState(GetSystemMenu(target, FALSE), command, MF_BYCOMMAND);
        if (state == UINT(-1) || (state & (MF_DISABLED | MF_GRAYED))) return;
        SetForegroundWindow(target); PostMessageW(target, WM_SYSCOMMAND, command, 0);
    }
    QLabel *title, *icon;
    QToolButton *minimize, *maximize, *closeButton;
    COLORREF oldBorder = 0;
    bool oldBorderValid = false;
    DWORD ownerPid = 0;
};

class Decorations final : public QObject
{
public:
    Decorations()
    {
        instance = this;
        locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        connect(&timer, &QTimer::timeout, this, [this] { sweep(); }); timer.start(750); sweep();
    }
    ~Decorations() override { instance = nullptr; if (locationHook) UnhookWinEvent(locationHook); if (foregroundHook) UnhookWinEvent(foregroundHook); qDeleteAll(bars); }
private:
    void sweep() {
        QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
        if (!settings.value(QStringLiteral("decorateWindows"), true).toBool()) { qDeleteAll(bars); bars.clear(); return; }
        QSet<HWND> live;
        EnumWindows([](HWND target, LPARAM data) -> BOOL { if (standardCaption(target)) reinterpret_cast<QSet<HWND>*>(data)->insert(target); return TRUE; }, reinterpret_cast<LPARAM>(&live));
        for (HWND target : bars.keys()) if (!live.contains(target)) delete bars.take(target);
        for (HWND target : live) { auto &bar = bars[target]; if (!bar) bar = new Caption(target); bar->updateCaption(); }
    }
    static void CALLBACK event(HWINEVENTHOOK, DWORD type, HWND target, LONG object, LONG, DWORD, DWORD) {
        if (!instance) return;
        if (type == EVENT_OBJECT_LOCATIONCHANGE && object == OBJID_WINDOW) {
            if (auto *bar = instance->bars.value(target)) bar->updateCaption();
        } else if (type == EVENT_SYSTEM_FOREGROUND) {
            for (auto *bar : instance->bars) bar->updateCaption();
        }
    }
    static inline Decorations *instance = nullptr;
    QHash<HWND, Caption*> bars;
    QTimer timer;
    HWINEVENTHOOK locationHook = nullptr, foregroundHook = nullptr;
};
static LRESULT CALLBACK fixtureProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCALCSIZE) return 0; // Client-drawn caption retaining WS_CAPTION.
    return DefWindowProcW(window, message, wparam, lparam);
}
int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")) qputenv("DBUS_SESSION_BUS_ADDRESS", "tcp:host=127.0.0.1,port=12443");
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows"); KIconTheme::initTheme(); QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        const HINSTANCE module = GetModuleHandleW(nullptr);
        WNDCLASSW klass{}; klass.hInstance = module; klass.lpfnWndProc = fixtureProcedure; klass.lpszClassName = L"KDEDecorationCustomFixture"; RegisterClassW(&klass);
        HWND native = CreateWindowExW(0, L"STATIC", L"Native fixture", WS_OVERLAPPEDWINDOW, 100,100,400,300,nullptr,nullptr,module,nullptr);
        HWND custom = CreateWindowExW(0, klass.lpszClassName, L"Custom fixture", WS_OVERLAPPEDWINDOW, 100,100,400,300,nullptr,nullptr,module,nullptr);
        const bool ok = native && custom && standardCaption(native, true) && !standardCaption(custom, true);
        if (native) DestroyWindow(native); if (custom) DestroyWindow(custom); UnregisterClassW(klass.lpszClassName,module);
        QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": real hidden native caption accepted; client-drawn caption rejected; no host windows decorated\n";
        return ok ? 0 : 1;
    }
    auto bus = QDBusConnection::sessionBus(); if (!bus.registerService(QStringLiteral("org.nixbridge.WindowDecorations"))) return 1;
    Decorations decorations; return app.exec();
}
