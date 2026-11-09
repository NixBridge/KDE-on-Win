// SPDX-License-Identifier: GPL-3.0-or-later
#include "localization.h"
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
#include <QFontMetrics>
#include <QAbstractNativeEventFilter>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QEventLoop>
#include <QElapsedTimer>
#include <KIconTheme>
#include <KIconLoader>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <oleacc.h>
#include <vector>
#include "appearance.h"
#include "../style/captionicons.h"

namespace {
struct PhysicalCoordinates {
    DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ~PhysicalCoordinates()
    {
        if (previous)
            SetThreadDpiAwarenessContext(previous);
    }
};
bool sameUserAndIntegrity(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return false;
    HANDLE remote = nullptr, local = nullptr;
    const bool opened
        = OpenProcessToken(process, TOKEN_QUERY, &remote) && OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &local);
    auto information = [](HANDLE token, TOKEN_INFORMATION_CLASS type) {
        DWORD size = 0;
        GetTokenInformation(token, type, nullptr, 0, &size);
        std::vector<BYTE> data(size);
        if (!size || !GetTokenInformation(token, type, data.data(), size, &size))
            data.clear();
        return data;
    };
    bool permitted = false;
    if (opened) {
        const auto user = information(remote, TokenUser), own = information(local, TokenUser);
        const auto level = information(remote, TokenIntegrityLevel), ownLevel = information(local, TokenIntegrityLevel);
        if (!user.empty() && !own.empty() && !level.empty() && !ownLevel.empty()) {
            const auto sid = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(level.data())->Label.Sid;
            const auto localSid = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(ownLevel.data())->Label.Sid;
            const DWORD integrity = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
            const DWORD localIntegrity = *GetSidSubAuthority(localSid, *GetSidSubAuthorityCount(localSid) - 1);
            permitted = EqualSid(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid,
                            reinterpret_cast<const TOKEN_USER*>(own.data())->User.Sid)
                && integrity <= localIntegrity;
        }
    }
    if (remote)
        CloseHandle(remote);
    if (local)
        CloseHandle(local);
    CloseHandle(process);
    return permitted;
}
bool standardCaption(HWND window, bool fixture = false, bool checkIdentity = true)
{
    if (!IsWindow(window) || (!fixture && (!IsWindowVisible(window) || IsIconic(window))))
        return false;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    if ((style & WS_CAPTION) != WS_CAPTION || (style & WS_CHILD)
        || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW))
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!fixture && pid == GetCurrentProcessId())
        return false;
    if (checkIdentity && !sameUserAndIntegrity(pid))
        return false;
    DWORD cloaked = 0;
    DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked)
        return false;
    RECT rect {};
    GetWindowRect(window, &rect);
    POINT client {};
    ClientToScreen(window, &client);
    const UINT dpi = GetDpiForWindow(window);
    const int caption = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
    // A client extended into the native caption is self-decorated, even if
    // it retains WS_CAPTION for DWM shadows and resizing.
    if (client.y - rect.top < caption)
        return false;
    MONITORINFO monitor {};
    monitor.cbSize = sizeof(monitor);
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)
        && rect.left <= monitor.rcMonitor.left && rect.top <= monitor.rcMonitor.top
        && rect.right >= monitor.rcMonitor.right && rect.bottom >= monitor.rcMonitor.bottom && !IsZoomed(window))
        return false;
    return true;
}
QRect captionRectangle(HWND target)
{
    PhysicalCoordinates coordinates;
    RECT bounds {};
    if (!GetWindowRect(target, &bounds))
        return {};
    RECT visible = bounds, extended {};
    if (SUCCEEDED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS, &extended, sizeof(extended)))
        && extended.right > extended.left && extended.bottom > extended.top)
        visible = extended;
    TITLEBARINFO info {};
    info.cbSize = sizeof(info);
    const UINT dpi = GetDpiForWindow(target);
    const LONG fallbackBottom = bounds.top + GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
        + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi) + GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
    if (!GetTitleBarInfo(target, &info) || info.rcTitleBar.bottom <= visible.top)
        info.rcTitleBar.bottom = fallbackBottom;
    // rcTitleBar can exclude the native system icon. Cover the entire visible
    // width, including that icon, while leaving menu/client content untouched.
    if (IsZoomed(target)) {
        MONITORINFO monitor { sizeof(monitor) };
        if (GetMonitorInfoW(MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST), &monitor)) {
            visible.left = qMax(visible.left, monitor.rcWork.left);
            visible.right = qMin(visible.right, monitor.rcWork.right);
            visible.top = qMax(visible.top, monitor.rcWork.top);
        }
    }
    return QRect(visible.left, visible.top, visible.right - visible.left, info.rcTitleBar.bottom - visible.top);
}
}

class Caption final : public QWidget {
    Q_OBJECT

public:
    explicit Caption(HWND target)
        : QWidget(
              nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint)
        , target(target)
    {
        GetWindowThreadProcessId(target, &ownerPid);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAutoFillBackground(true);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 0, 0, 0);
        layout->setSpacing(0);
        icon = new QLabel(this);
        icon->setFixedWidth(24);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(icon);
        DWORD_PTR nativeIcon = 0;
        SendMessageTimeoutW(target, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 30, &nativeIcon);
        if (!nativeIcon)
            nativeIcon = GetClassLongPtrW(target, GCLP_HICONSM);
        if (nativeIcon)
            icon->setPixmap(QPixmap::fromImage(QImage::fromHICON(reinterpret_cast<HICON>(nativeIcon)))
                    .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        title = new QLabel(this);
        title->setAlignment(Qt::AlignCenter);
        title->setMinimumWidth(0);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title, 1);
        auto button = [&](const QString& name, const QString& tip, UINT command) {
            auto* widget = new QToolButton(this);
            widget->setAutoRaise(true);
            widget->setFocusPolicy(Qt::NoFocus);
            widget->setToolTip(tip);
            widget->setFixedWidth(36);
            widget->setIcon(captionButtonIcon(name, palette()));
            layout->addWidget(widget);
            connect(widget, &QToolButton::clicked, this, [this, command] { act(command); });
            return widget;
        };
        minimize = button(QStringLiteral("window-minimize"), tr("Minimize"), SC_MINIMIZE);
        maximize = button(QStringLiteral("window-maximize"), tr("Maximize / Restore"), SC_MAXIMIZE);
        closeButton = button(QStringLiteral("window-close"), tr("Close"), SC_CLOSE);
        connect(qGuiApp, &QGuiApplication::paletteChanged, this, [this] {
            iconInitialized = false;
            updateCaption();
        });
        SetWindowLongPtrW(reinterpret_cast<HWND>(winId()), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(target));
        oldBorderValid = SUCCEEDED(DwmGetWindowAttribute(target, DWMWA_BORDER_COLOR, &oldBorder, sizeof(oldBorder)));
        // DWM captures only the owner during minimize, excluding this tool
        // window. Its native caption would flash through that animation.
        // This attribute is write-only: release restores Windows' default.
        const BOOL disabled = TRUE;
        transitionsDisabled
            = SUCCEEDED(DwmSetWindowAttribute(target, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled)));
        updateCaption();
    }
    ~Caption() override
    {
        if (transitionsDisabled && ownedTarget()) {
            const BOOL disabled = FALSE;
            DwmSetWindowAttribute(target, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled));
        }
        if (oldBorderValid && ownedTarget())
            DwmSetWindowAttribute(target, DWMWA_BORDER_COLOR, &oldBorder, sizeof(oldBorder));
    }
    void updateCaption()
    {
        PhysicalCoordinates coordinates;
        if (!ownedTarget() || !standardCaption(target, false, false)) {
            hide();
            return;
        }
        const QRect rect = captionRectangle(target);
        if (rect.isEmpty()) {
            hide();
            return;
        }
        QFont captionFont = qApp->font();
        const int available
            = qMax(10, int(rect.height() / (qMax(96u, GetDpiForWindow(reinterpret_cast<HWND>(winId()))) / 96.0)) - 6);
        if (QFontMetrics(captionFont).height() > available)
            captionFont.setPixelSize(qMax(9, available - 4));
        title->setFont(captionFont);
        // Native geometry is in physical pixels. Qt's HWND positioning below
        // avoids mixing coordinate origins on monitors with different scaling.
        wchar_t name[1024] {};
        GetWindowTextW(target, name, 1024);
        title->setText(QString::fromWCharArray(name));
        const bool active = GetForegroundWindow() == target;
        auto colors = qApp->palette();
        colors.setCurrentColorGroup(active ? QPalette::Active : QPalette::Inactive);
        setPalette(colors);
        const auto color = colors.color(QPalette::Window);
        const COLORREF border = RGB(color.red(), color.green(), color.blue());
        if (oldBorderValid && (!borderApplied || border != lastBorder)) {
            DwmSetWindowAttribute(target, DWMWA_BORDER_COLOR, &border, sizeof(border));
            lastBorder = border;
            borderApplied = true;
        }
        const LONG_PTR style = GetWindowLongPtrW(target, GWL_STYLE);
        minimize->setVisible(style & WS_MINIMIZEBOX);
        maximize->setVisible(style & WS_MAXIMIZEBOX);
        closeButton->setVisible(style & WS_SYSMENU);
        const UINT closeState = GetMenuState(GetSystemMenu(target, FALSE), SC_CLOSE, MF_BYCOMMAND);
        closeButton->setEnabled(closeState != UINT(-1) && !(closeState & (MF_DISABLED | MF_GRAYED)));
        const bool zoomed = IsZoomed(target);
        if (!iconInitialized || zoomed != lastZoomed || colors != iconPalette
            || colors.currentColorGroup() != iconPalette.currentColorGroup()) {
            minimize->setIcon(captionButtonIcon(QStringLiteral("window-minimize"), colors));
            closeButton->setIcon(captionButtonIcon(QStringLiteral("window-close"), colors));
            maximize->setIcon(captionButtonIcon(
                zoomed ? QStringLiteral("window-restore") : QStringLiteral("window-maximize"), colors));
            iconPalette = colors;
        }
        lastZoomed = zoomed;
        iconInitialized = true;
        if (!isVisible()) {
            lastRectangle = {};
            show();
        }
        const HWND handle = reinterpret_cast<HWND>(winId());
        if (GetWindow(handle, GW_OWNER) != target)
            SetWindowLongPtrW(handle, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(target));
        // Activation can raise the owner over its Qt tool window. Restore only
        // the foreground owner's decoration; never raise background captions.
        const bool raise = active && GetWindow(target, GW_HWNDPREV) != handle;
        const bool moved = rect != lastRectangle;
        if (raise || moved) {
            const HWND order = (GetWindowLongPtrW(target, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_TOP;
            SetWindowPos(handle, order, rect.x(), rect.y(), rect.width(), rect.height(),
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | (raise ? 0 : SWP_NOZORDER));
            lastRectangle = rect;
        }
    }
    bool minimizesInstantly() const { return transitionsDisabled; }
    bool retainWhileMinimized() const
    {
        return ownedTarget() && IsIconic(target) && (GetWindowLongPtrW(target, GWL_STYLE) & WS_CAPTION) == WS_CAPTION;
    }
    HWND target;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && ownedTarget()) {
            POINT cursor {};
            GetPhysicalCursorPos(&cursor);
            int hit = HTCAPTION;
            if (!IsZoomed(target) && (GetWindowLongPtrW(target, GWL_STYLE) & WS_THICKFRAME)) {
                const int edge = qMax(4, int(GetDpiForWindow(reinterpret_cast<HWND>(winId())) / 24));
                const bool left = cursor.x<lastRectangle.left() + edge, right = cursor.x> lastRectangle.right() - edge,
                           top = cursor.y < lastRectangle.top() + edge;
                if (top)
                    hit = left ? HTTOPLEFT : (right ? HTTOPRIGHT : HTTOP);
                else if (left)
                    hit = HTLEFT;
                else if (right)
                    hit = HTRIGHT;
            }
            PhysicalToLogicalPointForPerMonitorDPI(target, &cursor);
            SetForegroundWindow(target);
            updateCaption();
            ReleaseCapture();
            PostMessageW(target, WM_NCLBUTTONDOWN, hit, MAKELPARAM(cursor.x, cursor.y));
        }
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            act(SC_MAXIMIZE);
    }
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        QMenu menu;
        for (const auto command : { SC_RESTORE, SC_MINIMIZE, SC_MAXIMIZE, SC_CLOSE }) {
            const QString label = command == SC_RESTORE ? tr("Restore")
                : command == SC_MINIMIZE                ? tr("Minimize")
                : command == SC_MAXIMIZE                ? tr("Maximize")
                                                        : tr("Close");
            auto* action = menu.addAction(label);
            const UINT state = GetMenuState(GetSystemMenu(target, FALSE), command, MF_BYCOMMAND);
            action->setEnabled(state != UINT(-1) && !(state & (MF_DISABLED | MF_GRAYED)));
            connect(action, &QAction::triggered, this, [this, command] { act(command); });
        }
        menu.exec(event->globalPos());
    }

private:
    bool ownedTarget() const
    {
        DWORD pid = 0;
        return GetWindowThreadProcessId(target, &pid) && pid == ownerPid;
    }
    void act(UINT command)
    {
        if (!ownedTarget())
            return;
        if (command == SC_MAXIMIZE && IsZoomed(target))
            command = SC_RESTORE;
        if (command == SC_RESTORE) {
            ShowWindowAsync(target, SW_RESTORE);
            return;
        }
        const UINT state = GetMenuState(GetSystemMenu(target, FALSE), command, MF_BYCOMMAND);
        if (state == UINT(-1) || (state & (MF_DISABLED | MF_GRAYED)))
            return;
        SetForegroundWindow(target);
        PostMessageW(target, WM_SYSCOMMAND, command, 0);
    }
    QLabel *title, *icon;
    QToolButton *minimize, *maximize, *closeButton;
    COLORREF oldBorder = 0;
    bool oldBorderValid = false;
    bool transitionsDisabled = false;
    QPalette iconPalette;
    bool borderApplied = false, iconInitialized = false, lastZoomed = false;
    COLORREF lastBorder = 0;
    QRect lastRectangle;
    DWORD ownerPid = 0;
};

class Decorations final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nixbridge.WindowDecorations")
public:
    explicit Decorations(HWND hiddenFixture = nullptr)
        : fixture(hiddenFixture)
    {
        if (fixture)
            return; // Tests may attach only this hidden, test-owned HWND.
        instance = this;
        locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, event, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, event, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        showHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, event, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        dialogHook = SetWinEventHook(EVENT_SYSTEM_DIALOGSTART, EVENT_SYSTEM_DIALOGSTART, nullptr, event, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        destroyHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, event, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        connect(&timer, &QTimer::timeout, this, [this] { sweep(); });
        timer.start(1500);
        sweep();
    }
    ~Decorations() override
    {
        if (!fixture)
            instance = nullptr;
        for (auto hook : { showHook, dialogHook, destroyHook })
            if (hook)
                UnhookWinEvent(hook);
        if (locationHook)
            UnhookWinEvent(locationHook);
        if (foregroundHook)
            UnhookWinEvent(foregroundHook);
        qDeleteAll(bars);
    }

    void observe(HWND target)
    {
        if (!target || (fixture && target != fixture) || pending.contains(target))
            return;
        pending.insert(target);
        // SHOW/FOREGROUND may precede final frame geometry. Retry only this
        // candidate briefly instead of waiting for the 1.5-second full sweep.
        tryCandidate(target, 0);
    }
    bool tracks(HWND target) const { return bars.contains(target); }
public Q_SLOTS:
    void Quit() { QCoreApplication::quit(); }

private:
    void tryCandidate(HWND target, int attempt)
    {
        QTimer::singleShot(attempt ? 25 : 0, this, [this, target, attempt] {
            if (!pending.contains(target))
                return;
            QSettings preferences(appearanceSettingsPath(), QSettings::IniFormat);
            const bool enabled = preferences.value("decorateWindows", true).toBool();
            if (enabled && IsWindow(target) && standardCaption(target, target == fixture)) {
                auto& bar = bars[target];
                if (!bar)
                    bar = new Caption(target);
                bar->updateCaption();
                pending.remove(target);
            } else if (enabled && IsWindow(target) && attempt < 3) {
                tryCandidate(target, attempt + 1);
            } else {
                pending.remove(target);
            }
        });
    }

    void sweep()
    {
        QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
        if (!settings.value(QStringLiteral("decorateWindows"), true).toBool()) {
            qDeleteAll(bars);
            bars.clear();
            return;
        }
        QSet<HWND> live;
        EnumWindows(
            [](HWND target, LPARAM data) -> BOOL {
                if (standardCaption(target))
                    reinterpret_cast<QSet<HWND>*>(data)->insert(target);
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&live));
        for (HWND target : bars.keys()) {
            // Keep the animation policy attached across minimization. The
            // overlay itself hides, but must not be destroyed by the sweep.
            if (bars.value(target)->retainWhileMinimized())
                live.insert(target);
            else if (!live.contains(target))
                delete bars.take(target);
        }
        for (HWND target : live) {
            auto& bar = bars[target];
            if (!bar)
                bar = new Caption(target);
            bar->updateCaption();
        }
    }
    static void CALLBACK event(HWINEVENTHOOK, DWORD type, HWND target, LONG object, LONG, DWORD, DWORD)
    {
        if (!instance)
            return;
        if ((type == EVENT_OBJECT_SHOW && object == OBJID_WINDOW) || type == EVENT_SYSTEM_DIALOGSTART) {
            instance->observe(target);
        } else if (type == EVENT_OBJECT_DESTROY && object == OBJID_WINDOW) {
            instance->pending.remove(target);
            delete instance->bars.take(target);
        } else if (type == EVENT_OBJECT_LOCATIONCHANGE && object == OBJID_WINDOW) {
            if (auto* bar = instance->bars.value(target))
                bar->updateCaption();
        } else if (type == EVENT_SYSTEM_FOREGROUND) {
            if (auto* previous = instance->bars.value(instance->lastForeground))
                previous->updateCaption();
            if (auto* current = instance->bars.value(target))
                current->updateCaption();
            instance->lastForeground = target;
            instance->observe(target);
        }
    }
    static inline Decorations* instance = nullptr;
    QHash<HWND, Caption*> bars;
    QTimer timer;
    HWINEVENTHOOK locationHook = nullptr, foregroundHook = nullptr;
    HWND lastForeground = nullptr;
    HWND fixture = nullptr;
    QSet<HWND> pending;
    HWINEVENTHOOK showHook = nullptr, dialogHook = nullptr, destroyHook = nullptr;
};
static LRESULT CALLBACK fixtureProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCALCSIZE)
        return 0; // Client-drawn caption retaining WS_CAPTION.
    return DefWindowProcW(window, message, wparam, lparam);
}
int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"))
        qputenv("DBUS_SESSION_BUS_ADDRESS", "tcp:host=127.0.0.1,port=12443");
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    installProjectTranslations(&app);
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        PhysicalCoordinates coordinates;
        const HINSTANCE module = GetModuleHandleW(nullptr);
        WNDCLASSW klass {};
        klass.hInstance = module;
        klass.lpfnWndProc = fixtureProcedure;
        klass.lpszClassName = L"KDEDecorationCustomFixture";
        RegisterClassW(&klass);
        HWND native = CreateWindowExW(0, L"STATIC", L"Native fixture", WS_OVERLAPPEDWINDOW, 100, 100, 400, 300, nullptr,
            nullptr, module, nullptr);
        HWND custom = CreateWindowExW(0, klass.lpszClassName, L"Custom fixture", WS_OVERLAPPEDWINDOW, 100, 100, 400,
            300, nullptr, nullptr, module, nullptr);
        bool ok = native && custom && standardCaption(native, true) && !standardCaption(custom, true);
        if (native) {
            {
                Decorations rapid(native);
                QElapsedTimer elapsed;
                elapsed.start();
                rapid.observe(native);
                rapid.observe(native); // Coalesce duplicate SHOW/FOREGROUND events.
                QEventLoop loop;
                QTimer::singleShot(150, &loop, &QEventLoop::quit);
                loop.exec();
                ok &= rapid.tracks(native) && elapsed.elapsed() < 500;
            }
            Caption nativeBar(native);
            ok &= nativeBar.minimizesInstantly();
            RECT visible {};
            GetWindowRect(native, &visible);
            DwmGetWindowAttribute(native, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible));
            const auto rectangle = captionRectangle(native);
            ok &= !rectangle.isEmpty() && rectangle.left() <= visible.left && rectangle.right() + 1 >= visible.right;
            HWND child = CreateWindowExW(0, L"STATIC", L"Restore fixture",
                WS_CHILD | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_MAXIMIZE, 0, 0, 100, 80,
                native, nullptr, module, nullptr);
            if (child) {
                Caption bar(child);
                ShowWindow(child, SW_MINIMIZE);
                ok &= bar.retainWhileMinimized();
                auto buttons = bar.findChildren<QToolButton*>();
                ShowWindow(child, SW_MAXIMIZE);
                ok &= buttons.size() == 3 && IsZoomed(child);
                if (buttons.size() == 3)
                    buttons[1]->click();
                for (int i = 0; i < 10; ++i) {
                    QCoreApplication::processEvents();
                    Sleep(5);
                }
                ok &= !IsZoomed(child) && !IsWindowVisible(child) && !IsWindowVisible(native);
                DestroyWindow(child);
            } else
                ok = false;
        }
        if (native)
            DestroyWindow(native);
        if (custom)
            DestroyWindow(custom);
        UnregisterClassW(klass.lpszClassName, module);
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": event-driven caption attachment, physical full-caption bounds, instant-transition "
                               "policy, minimized tracking, restore "
                               "button and custom-caption exclusion on hidden "
                               "test-owned windows\n";
        return ok ? 0 : 1;
    }
    auto bus = QDBusConnection::sessionBus();
    bool registered = bus.registerService(QStringLiteral("org.nixbridge.WindowDecorations"));
    if (!registered && app.arguments().contains(QStringLiteral("--replace"))) {
        QDBusInterface previous(QStringLiteral("org.nixbridge.WindowDecorations"), QStringLiteral("/Decorations"),
            QStringLiteral("org.nixbridge.WindowDecorations"), bus);
        previous.call(QStringLiteral("Quit"));
        for (int retry = 0; retry < 20 && !registered; ++retry) {
            Sleep(25);
            registered = bus.registerService(QStringLiteral("org.nixbridge.WindowDecorations"));
        }
    }
    if (!registered)
        return 1;
    Decorations decorations;
    bus.registerObject(QStringLiteral("/Decorations"), &decorations, QDBusConnection::ExportAllSlots);
    return app.exec();
}

#include "window-decorations.moc"
