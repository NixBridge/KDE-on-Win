// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QStylePlugin>
#include <QStyleFactory>
#include <QProxyStyle>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>
#include <QTimer>
#include <QFileInfo>
#include <QFile>
#include <QAbstractNativeEventFilter>
#include <KIconLoader>
#include <KColorScheme>
#include <KSharedConfig>
#include "../apps/appearance.h"
#include <windows.h>
#include <windowsx.h>

static void traceAppearance(const QString &message)
{
    const QString path=qEnvironmentVariable("PLASMA_BREEZE_TRACE");
    if(path.isEmpty())return;
    QFile file(path);if(file.open(QIODevice::WriteOnly|QIODevice::Append))file.write((message+QLatin1Char('\n')).toUtf8());
}

class WindowChrome final : public QObject, public QAbstractNativeEventFilter
{
public:
    explicit WindowChrome(QMainWindow *window) : QObject(window), m_window(window)
    {
        m_bar = new QWidget(window);
        m_bar->setObjectName("PlasmaBreezeTitleBar");
        m_bar->setAutoFillBackground(true);
        m_height = qMax(32, window->fontMetrics().height() + 12);
        auto *layout = new QHBoxLayout(m_bar);
        layout->setContentsMargins(8, 0, 0, 0);
        layout->setSpacing(0);
        auto *icon = new QLabel(m_bar);
        icon->setPixmap(window->windowIcon().pixmap(20,20));
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(icon);
        auto *title = new QLabel(window->windowTitle(), m_bar);
        title->setAlignment(Qt::AlignCenter);
        title->setMinimumWidth(0);
        title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title, 1);
        connect(window, &QWidget::windowTitleChanged, title, &QLabel::setText);
        connect(window, &QWidget::windowIconChanged, icon, [icon](const QIcon &value) { icon->setPixmap(value.pixmap(20,20)); });
        auto button = [&](const QString &name, const QString &tooltip) {
            auto *b = new QToolButton(m_bar);
            b->setIcon(KIconLoader::global()->loadIcon(name, KIconLoader::Toolbar, 16));
            b->setToolTip(tooltip); b->setAutoRaise(true); b->setFocusPolicy(Qt::NoFocus); b->setFixedSize(36,m_height);
            layout->addWidget(b); return b;
        };
        connect(button("window-minimize", tr("Minimize")), &QToolButton::clicked, window, &QWidget::showMinimized);
        m_maximize = button("window-maximize", tr("Maximize / Restore"));
        connect(m_maximize, &QToolButton::clicked, this, [this] { toggleMaximize(); });
        connect(button("window-close", tr("Close")), &QToolButton::clicked, window, &QWidget::close);
        m_margins = window->contentsMargins();
        window->setContentsMargins(m_margins.left(), m_margins.top()+m_height, m_margins.right(), m_margins.bottom());
        const bool visible = window->isVisible();
        const auto state = window->windowState();
        window->setWindowFlag(Qt::FramelessWindowHint, true);
        window->installEventFilter(this); m_bar->installEventFilter(this);
        const HWND handle=reinterpret_cast<HWND>(window->winId());
        m_handle=handle;
        qApp->installNativeEventFilter(this);
        if(IsWindow(handle)) {
            const auto nativeStyle=GetWindowLongPtrW(handle,GWL_STYLE);
            SetWindowLongPtrW(handle,GWL_STYLE,(nativeStyle|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)&~WS_CAPTION);
            SetWindowPos(handle,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
        if (visible) { window->setWindowState(state); window->show(); }
        update();
        traceAppearance(QStringLiteral("Chrome ready: ")+QLatin1String(window->metaObject()->className()));
    }
    ~WindowChrome() override { if (qApp) qApp->removeNativeEventFilter(this); }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if(object==m_window && event->type()==QEvent::WinIdChange)m_handle=reinterpret_cast<HWND>(m_window->effectiveWinId());
        if (object == m_bar && event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && m_window->windowHandle()) return m_window->windowHandle()->startSystemMove();
        }
        if (object == m_bar && event->type() == QEvent::MouseButtonDblClick) {
            if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) { toggleMaximize(); return true; }
        }
        if (object == m_window && (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange)) update();
        return false;
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override
    {
        auto *msg = static_cast<MSG *>(message);
        if (!m_handle || msg->hwnd != m_handle) return false;
        if(msg->message==WM_NCCALCSIZE && msg->wParam) {
            if(IsZoomed(msg->hwnd) && !m_window->isFullScreen()) {
                MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
                if(GetMonitorInfoW(MonitorFromWindow(msg->hwnd,MONITOR_DEFAULTTONEAREST),&monitor)) reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam)->rgrc[0]=monitor.rcWork;
            }
            *result=0;return true;
        }
        if (!m_window->isVisible() || msg->message != WM_NCHITTEST || m_window->isMaximized() || m_window->isFullScreen()) return false;
        RECT rect{}; GetWindowRect(msg->hwnd, &rect);
        const int edge = qMax(4, qRound(6*m_window->devicePixelRatioF()));
        const int x = GET_X_LPARAM(msg->lParam), y = GET_Y_LPARAM(msg->lParam);
        const bool left=x<rect.left+edge, right=x>=rect.right-edge, top=y<rect.top+edge, bottom=y>=rect.bottom-edge;
        const int hit = top ? (left?HTTOPLEFT:right?HTTOPRIGHT:HTTOP) : bottom ? (left?HTBOTTOMLEFT:right?HTBOTTOMRIGHT:HTBOTTOM) : left?HTLEFT:right?HTRIGHT:HTCLIENT;
        if (hit == HTCLIENT) return false;
        *result=hit; return true;
    }
private:
    void toggleMaximize() { m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized(); }
    void update() {
        if(m_window->isFullScreen()){m_bar->hide();if(m_window->contentsMargins()!=m_margins)m_window->setContentsMargins(m_margins);return;}
        const QMargins margins(m_margins.left(),m_margins.top()+m_height,m_margins.right(),m_margins.bottom());
        if(m_window->contentsMargins()!=margins)m_window->setContentsMargins(margins);
        m_bar->setGeometry(0,0,m_window->width(),m_height); m_bar->show(); m_bar->raise();
        m_maximize->setIcon(KIconLoader::global()->loadIcon(m_window->isMaximized()?"window-restore":"window-maximize", KIconLoader::Toolbar,16));
    }
    QMainWindow *m_window;
    QWidget *m_bar;
    QToolButton *m_maximize;
    int m_height;
    QMargins m_margins;
    HWND m_handle=nullptr;
};

class PlasmaWindowsStyle final : public QProxyStyle
{
    static QStyle *base() {if(auto *style=QStyleFactory::create("breeze"))return style;return QStyleFactory::create("Fusion");}
public:
    PlasmaWindowsStyle() : QProxyStyle(base()) {traceAppearance(QStringLiteral("Breeze style loaded"));}
    void polish(QApplication *application) override {
        QProxyStyle::polish(application);
        auto update = [application] {
            QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
            const bool dark = settings.value(QStringLiteral("dark"), false).toBool();
            if (application->property("plasmaAppearanceDark").isValid() && application->property("plasmaAppearanceDark").toBool() == dark) return;
            const QString path = application->applicationDirPath() + QStringLiteral("/data/color-schemes/") + (dark ? QStringLiteral("BreezeDark.colors") : QStringLiteral("BreezeLight.colors"));
            if (QFileInfo::exists(path)) application->setPalette(KColorScheme::createApplicationPalette(KSharedConfig::openConfig(path, KConfig::SimpleConfig)));
            application->setProperty("plasmaAppearanceDark", dark);
        };
        auto *timer = new QTimer(application); QObject::connect(timer, &QTimer::timeout, application, update); timer->start(1000);
        QTimer::singleShot(0, application, update);
    }
    void polish(QWidget *widget) override {
        QProxyStyle::polish(widget);
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("plasmaBreezeChrome").toBool()) return;
        const QString executable=QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower();
        if (!QStringList{"konsole.exe","dolphin.exe","plasma-process-monitor.exe","plasma-text-editor.exe","plasma-system-settings.exe","breeze-window-probe.exe"}.contains(executable)) return;
        window->setProperty("plasmaBreezeChrome",true);
        // Polish can run during construction; wait until the main window exists.
        QTimer::singleShot(0,window,[window] { new WindowChrome(window); });
    }
};
class PlasmaWindowsStylePlugin final : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "style.json")
public:
    QStyle *create(const QString &key) override { return key.compare("plasmawindows",Qt::CaseInsensitive)==0 ? new PlasmaWindowsStyle : nullptr; }
};
#include "plasmawindowsstyle.moc"
