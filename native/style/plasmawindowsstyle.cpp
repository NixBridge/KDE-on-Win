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
#include <QDateTime>
#include <QFile>
#include <QAbstractNativeEventFilter>
#include <KIconLoader>
#include <KColorScheme>
#include <KSharedConfig>
#include <KConfigGroup>
#include "../apps/appearance.h"
#include "captionicons.h"
#include "../apps/localization.h"
#include <windows.h>
#include <windowsx.h>

static void traceAppearance(const QString& message)
{
    const QString path = qEnvironmentVariable("PLASMA_BREEZE_TRACE");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
        file.write((message + QLatin1Char('\n')).toUtf8());
}

class WindowChrome final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit WindowChrome(QMainWindow* window)
        : QObject(window)
        , m_window(window)
    {
        m_bar = new QWidget(window);
        m_bar->setObjectName("PlasmaBreezeTitleBar");
        m_bar->setAutoFillBackground(true);
        m_height = qMax(32, window->fontMetrics().height() + 12);
        auto* layout = new QHBoxLayout(m_bar);
        layout->setContentsMargins(8, 0, 0, 0);
        layout->setSpacing(0);
        auto* icon = new QLabel(m_bar);
        icon->setPixmap(window->windowIcon().pixmap(20, 20));
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(icon);
        auto* title = new QLabel(window->windowTitle(), m_bar);
        title->setAlignment(Qt::AlignCenter);
        title->setMinimumWidth(0);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title, 1);
        connect(window, &QWidget::windowTitleChanged, title, &QLabel::setText);
        connect(window, &QWidget::windowIconChanged, icon,
            [icon](const QIcon& value) { icon->setPixmap(value.pixmap(20, 20)); });
        auto button = [&](const QString& name, const QString& tooltip) {
            auto* b = new QToolButton(m_bar);
            b->setIcon(captionButtonIcon(name, m_bar->palette()));
            b->setToolTip(tooltip);
            b->setAutoRaise(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedSize(36, m_height);
            layout->addWidget(b);
            return b;
        };
        m_minimize = button("window-minimize", tr("Minimize"));
        connect(m_minimize, &QToolButton::clicked, window, &QWidget::showMinimized);
        m_maximize = button("window-maximize", tr("Maximize / Restore"));
        connect(m_maximize, &QToolButton::clicked, this, [this] { toggleMaximize(); });
        m_close = button("window-close", tr("Close"));
        connect(m_close, &QToolButton::clicked, window, &QWidget::close);
        m_margins = window->contentsMargins();
        window->setContentsMargins(m_margins.left(), m_margins.top() + m_height, m_margins.right(), m_margins.bottom());
        const bool visible = window->isVisible();
        const auto state = window->windowState();
        window->setWindowFlag(Qt::FramelessWindowHint, true);
        window->installEventFilter(this);
        m_bar->installEventFilter(this);
        const HWND handle = reinterpret_cast<HWND>(window->winId());
        m_handle = handle;
        qApp->installNativeEventFilter(this);
        if (IsWindow(handle)) {
            const auto nativeStyle = GetWindowLongPtrW(handle, GWL_STYLE);
            SetWindowLongPtrW(handle, GWL_STYLE,
                (nativeStyle | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX) & ~WS_CAPTION);
            SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        if (visible) {
            window->setWindowState(state);
            window->show();
        }
        update();
        traceAppearance(QStringLiteral("Chrome ready: ") + QLatin1String(window->metaObject()->className()));
    }
    ~WindowChrome() override
    {
        if (qApp)
            qApp->removeNativeEventFilter(this);
    }
    bool eventFilter(QObject* object, QEvent* event) override
    {
        // Parent PaletteChange can arrive before the titlebar inherits it.
        // Refresh again when the actual painted widget has its new palette.
        if (object == m_bar
            && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange))
            update();
        if (object == m_window && event->type() == QEvent::WinIdChange) {
            m_handle = reinterpret_cast<HWND>(m_window->effectiveWinId());
            QTimer::singleShot(0, this, [this] { repairFrame(); });
        }
        if (object == m_bar && event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton && m_window->windowHandle())
                return m_window->windowHandle()->startSystemMove();
        }
        if (object == m_bar && event->type() == QEvent::MouseButtonDblClick) {
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                toggleMaximize();
                return true;
            }
        }
        if (object == m_window
            && (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange
                || event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange
                || event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::WindowActivate
                || event->type() == QEvent::WindowDeactivate))
            update();
        return false;
    }
    bool nativeEventFilter(const QByteArray&, void* message, qintptr* result) override
    {
        auto* msg = static_cast<MSG*>(message);
        if (!m_handle || msg->hwnd != m_handle)
            return false;
        if (msg->message == WM_STYLECHANGED)
            QTimer::singleShot(0, this, [this] { repairFrame(); });
        if (msg->message == WM_NCCALCSIZE && msg->wParam) {
            if (IsZoomed(msg->hwnd) && !m_window->isFullScreen()) {
                MONITORINFO monitor {};
                monitor.cbSize = sizeof(monitor);
                if (GetMonitorInfoW(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                    reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam)->rgrc[0] = monitor.rcWork;
            }
            *result = 0;
            return true;
        }
        if (!m_window->isVisible() || msg->message != WM_NCHITTEST || m_window->isMaximized()
            || m_window->isFullScreen())
            return false;
        RECT rect {};
        GetWindowRect(msg->hwnd, &rect);
        const int edge = qMax(4, qRound(6 * m_window->devicePixelRatioF()));
        const int x = GET_X_LPARAM(msg->lParam), y = GET_Y_LPARAM(msg->lParam);
        const bool left = x < rect.left + edge, right = x >= rect.right - edge, top = y < rect.top + edge,
                   bottom = y >= rect.bottom - edge;
        const int hit = top ? (left           ? HTTOPLEFT
                                      : right ? HTTOPRIGHT
                                              : HTTOP)
            : bottom        ? (left           ? HTBOTTOMLEFT
                                      : right ? HTBOTTOMRIGHT
                                              : HTBOTTOM)
            : left          ? HTLEFT
            : right         ? HTRIGHT
                            : HTCLIENT;
        if (hit == HTCLIENT)
            return false;
        *result = hit;
        return true;
    }

private:
    void repairFrame()
    {
        if (!IsWindow(m_handle) || m_window->isFullScreen())
            return;
        const auto style = GetWindowLongPtrW(m_handle, GWL_STYLE);
        const auto wanted = (style | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX) & ~WS_CAPTION;
        if (style == wanted)
            return;
        SetWindowLongPtrW(m_handle, GWL_STYLE, wanted);
        SetWindowPos(
            m_handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    void toggleMaximize()
    {
        const bool restore = m_window->isMaximized() || (IsWindow(m_handle) && IsZoomed(m_handle));
        if (restore) {
            m_window->showNormal();
            // Native maximization (snap/system menu) can precede Qt's state.
            if (IsWindow(m_handle) && IsZoomed(m_handle))
                ShowWindowAsync(m_handle, SW_RESTORE);
        } else
            m_window->showMaximized();
    }
    void update()
    {
        repairFrame();
        const int height = qMax(32, m_window->fontMetrics().height() + 12);
        if (height != m_height) {
            m_height = height;
            for (auto* button : m_bar->findChildren<QToolButton*>())
                button->setFixedHeight(m_height);
        }
        if (m_window->isFullScreen()) {
            m_bar->hide();
            if (m_window->contentsMargins() != m_margins)
                m_window->setContentsMargins(m_margins);
            return;
        }
        const QMargins margins(m_margins.left(), m_margins.top() + m_height, m_margins.right(), m_margins.bottom());
        if (m_window->contentsMargins() != margins)
            m_window->setContentsMargins(margins);
        m_bar->setGeometry(0, 0, m_window->width(), m_height);
        m_bar->show();
        m_bar->raise();
        auto colors = m_bar->palette();
        colors.setCurrentColorGroup(m_window->isActiveWindow() ? QPalette::Active : QPalette::Inactive);
        const bool maximized = m_window->isMaximized() || (IsWindow(m_handle) && IsZoomed(m_handle));
        if (!m_iconsInitialized || colors != m_iconPalette || maximized != m_iconMaximized
            || colors.currentColorGroup() != m_iconPalette.currentColorGroup()) {
            m_minimize->setIcon(captionButtonIcon("window-minimize", colors));
            m_close->setIcon(captionButtonIcon("window-close", colors));
            m_maximize->setIcon(captionButtonIcon(maximized ? "window-restore" : "window-maximize", colors));
            m_iconPalette = colors;
            m_iconMaximized = maximized;
            m_iconsInitialized = true;
        }
    }
    QMainWindow* m_window;
    QWidget* m_bar;
    QToolButton *m_maximize, *m_minimize, *m_close;
    int m_height;
    QMargins m_margins;
    HWND m_handle = nullptr;
    QPalette m_iconPalette;
    bool m_iconsInitialized = false, m_iconMaximized = false;
};

class PlasmaWindowsStyle final : public QProxyStyle {
    static QStyle* base()
    {
        if (auto* style = QStyleFactory::create("breeze"))
            return style;
        return QStyleFactory::create("Fusion");
    }

public:
    PlasmaWindowsStyle()
        : QProxyStyle(base())
    {
        traceAppearance(QStringLiteral("Breeze style loaded"));
    }
    void polish(QApplication* application) override
    {
        QProxyStyle::polish(application);
        installProjectTranslations(application);
        auto update = [application] {
            QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
            const bool dark = settings.value(QStringLiteral("dark"), false).toBool();
            const int fontSize = qBound(8, settings.value(QStringLiteral("fontSize"), 10).toInt(), 20);
            const QString accentValue = settings.value("accentColor").toString();
            const QString family = settings.value("fontFamily", defaultProjectFontFamily()).toString();
            const QString localFile = QFileInfo(appearanceSettingsPath()).absolutePath() + "/current.colors";
            const QString expected = dark ? QStringLiteral("BreezeDark") : QStringLiteral("BreezeLight");
            QString path = application->applicationDirPath() + "/data/color-schemes/" + expected + ".colors";
            if (QFileInfo::exists(localFile)) {
                auto local = KSharedConfig::openConfig(localFile);
                local->reparseConfiguration();
                if (local->group("General").readEntry("ColorScheme", QString()) == expected)
                    path = localFile;
            }
            const QString state = QString::number(dark) + ':' + QString::number(fontSize) + accentValue + family + path
                + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
            if (application->property("plasmaAppearanceState").toString() == state)
                return;
            if (QFileInfo::exists(path)) {
                auto configuration = KSharedConfig::openConfig(path);
                configuration->reparseConfiguration();
                application->setProperty("KDE_COLOR_SCHEME_PATH", path);
                QPalette palette = KColorScheme::createApplicationPalette(configuration);
                palette.setColor(QPalette::Accent, palette.color(QPalette::Highlight));
                QColor accent(accentValue);
                if (accent.isValid()) {
                    palette.setColor(QPalette::Highlight, accent);
                    palette.setColor(QPalette::Accent, accent);
                    palette.setColor(QPalette::Link, accent);
                    palette.setColor(QPalette::HighlightedText, accent.lightnessF() > 0.6 ? Qt::black : Qt::white);
                }
                KIconLoader::global()->setCustomPalette(palette);
                application->setPalette(palette);
                // Color-scheme paths can change without changing QPalette.
                QEvent colorsChanged(QEvent::ApplicationPaletteChange);
                QCoreApplication::sendEvent(application, &colorsChanged);
            }
            application->setProperty("plasmaAppearanceDark", dark);
            application->setProperty("plasmaAppearanceState", state);
            QFont font = application->font();
            font.setPointSize(fontSize);
            if (!family.isEmpty())
                font.setFamily(family);
            application->setFont(font);
        };
        auto* timer = new QTimer(application);
        QObject::connect(timer, &QTimer::timeout, application, update);
        timer->start(1000);
        QTimer::singleShot(0, application, update);
    }
    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        auto* window = qobject_cast<QMainWindow*>(widget);
        if (!window || window->property("plasmaBreezeChrome").toBool())
            return;
        const QString executable = QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower();
        if (!QStringList { "plasma-archive-manager.exe", "konsole.exe", "dolphin.exe", "plasma-process-monitor.exe",
                "plasma-text-editor.exe", "plasma-system-settings.exe", "plasma-open-with.exe",
                "breeze-window-probe.exe" }
                .contains(executable))
            return;
        window->setProperty("plasmaBreezeChrome", true);
        // Polish can run during construction; wait until the main window exists.
        QTimer::singleShot(0, window, [window] { new WindowChrome(window); });
    }
};
class PlasmaWindowsStylePlugin final : public QStylePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "style.json")
public:
    QStyle* create(const QString& key) override
    {
        return key.compare("plasmawindows", Qt::CaseInsensitive) == 0 ? new PlasmaWindowsStyle : nullptr;
    }
};
#include "plasmawindowsstyle.moc"
