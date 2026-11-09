// SPDX-License-Identifier: GPL-3.0-or-later
#include "localization.h"
#include "project-about.h"
#include "clipboard-history.h"
#include "desktop-toggle.h"
#include "windowsinstancelock.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QAbstractNativeEventFilter>
#include <tlhelp32.h>
#include <QClipboard>
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QCloseEvent>
#include <QAction>
#include <QTimer>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTemporaryDir>
#include <KIconTheme>

static QString channel()
{
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    return "KDE-on-Win.DesktopTools-" + QString::number(session) + '-'
        + QString::fromLatin1(
            QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}
static bool desktopPresent()
{
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return true; // Failure is not proof the desktop exited.
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry))
        do {
            DWORD other = 0;
            if (!_wcsicmp(entry.szExeFile, L"plasmashell.exe") && ProcessIdToSessionId(entry.th32ProcessID, &other)
                && other == session) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return found;
}
static bool toolsSessionEnded(bool daemon, bool seen, bool alive, qint64 elapsed)
{
    return daemon && !alive && (seen || elapsed >= 60000);
}
static constexpr int hotkeyBase = 0x4b00;
class DesktopTools final : public QMainWindow, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit DesktopTools(bool testing = false)
        : test(testing)
    {
        setWindowTitle(tr("Desktop Tools and Clipboard"));
        setWindowIcon(QIcon::fromTheme("edit-paste"));
        resize(640, 500);
        auto* body = new QWidget(this);
        auto* layout = new QVBoxLayout(body);
        setCentralWidget(body);
        auto* privacy = new QLabel(tr("Text history stays in memory. Explicit privacy markers are respected; not all "
                                      "passwords can be identified."));
        privacy->setObjectName("ClipboardPrivacy");
        privacy->setWordWrap(true);
        layout->addWidget(privacy);
        items = new QListWidget(body);
        items->setAccessibleName(tr("Clipboard history"));
        layout->addWidget(items, 1);
        emptyHint = new QLabel;
        emptyHint->setWordWrap(true);
        layout->addWidget(emptyHint);
        auto* buttons = new QHBoxLayout;
        layout->addLayout(buttons);
        auto* use = new QPushButton(tr("Copy selected"));
        copyButton = use;
        copyButton->setEnabled(false);
        connect(items, &QListWidget::currentRowChanged, this, [this](int row) { copyButton->setEnabled(row >= 0); });
        auto* clear = new QPushButton(tr("Clear history"));
        clearButton = clear;
        clear->setEnabled(false);
        buttons->addWidget(use);
        buttons->addWidget(clear);
        connect(use, &QPushButton::clicked, this, [this] { select(); });
        connect(items, &QListWidget::itemActivated, this, [this] { select(); });
        connect(clear, &QPushButton::clicked, this, [this] {
            history.clear();
            refresh();
        });
        QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
        auto* enabled = new QCheckBox(tr("Keep text history during this session"));
        const bool configuredHistory = settings.value("clipboardEnabled", true).toBool();
        preferenceError = settings.status() != QSettings::NoError;
        enabled->setChecked(configuredHistory && !preferenceError);
        history.setEnabled(enabled->isChecked());
        layout->addWidget(enabled);
        auto* count = new QSpinBox;
        count->setRange(1, 200);
        count->setPrefix(tr("History limit: "));
        count->setValue(qBound(1, settings.value("clipboardHistoryLimit", 20).toInt(), 200));
        history.setLimit(count->value());
        layout->addWidget(count);
        hotkeys = new QComboBox;
        hotkeys->addItem(tr("Shortcuts: Ctrl+Alt+E/R/D/V"));
        hotkeys->addItem(tr("Shortcuts: Ctrl+Alt+Shift+E/R/D/V"));
        hotkeys->setCurrentIndex(settings.value("shortcutExtraShift", true).toBool() ? 1 : 0);
        layout->addWidget(hotkeys);
        auto* note = new QLabel(
            tr("Windows reserves Meta+E/R/D. These remain native; use E for Dolphin, R to run, D "
               "for desktop, V for history with the selected modifiers. Ctrl+Alt can conflict with AltGr."));
        note->setObjectName("ShortcutPolicy");
        note->setWordWrap(true);
        layout->addWidget(note);
        status = new QLabel;
        status->setWordWrap(true);
        layout->addWidget(status);
        auto* quit = new QPushButton(tr("Quit desktop tools"));
        layout->addWidget(quit);
        connect(quit, &QPushButton::clicked, this, [this] { shutdown(); });
        connect(enabled, &QCheckBox::toggled, this, [this](bool value) {
            history.setEnabled(value);
            save("clipboardEnabled", value);
            refresh();
        });
        connect(count, &QSpinBox::valueChanged, this, [this](int value) {
            history.setLimit(value);
            save("clipboardHistoryLimit", value);
            refresh();
        });
        connect(hotkeys, &QComboBox::currentIndexChanged, this, [this](int value) {
            save("shortcutExtraShift", value == 1);
            if (!test)
                registerKeys();
        });
        auto* escape = new QAction(this);
        escape->setShortcut(QKeySequence(Qt::Key_Escape));
        addAction(escape);
        connect(escape, &QAction::triggered, this, &QWidget::hide);
        addProjectAbout(this);
        qApp->installNativeEventFilter(this);
    }
    ~DesktopTools() override
    {
        qApp->removeNativeEventFilter(this);
        unregisterKeys();
        server.close();
    }
    bool start()
    {
        if (server.isListening())
            return true;
        if (test)
            pipe += "-fixture-" + QString::number(GetCurrentProcessId());
        server.setSocketOptions(QLocalServer::UserAccessOption);
        QLocalServer::removeServer(pipe); // Caller already holds the session instance lock.
        if (!server.listen(pipe))
            return false;
        connect(&server, &QLocalServer::newConnection, this, [this] {
            while (auto* socket = server.nextPendingConnection()) {
                socket->setReadBufferSize(2049);
                connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                const auto consume = [this, socket] {
                    if (socket->property("handled").toBool())
                        return;
                    auto bytes = socket->property("request").toByteArray() + socket->readAll();
                    if (bytes.size() > 2048) {
                        socket->abort();
                        return;
                    }
                    socket->setProperty("request", bytes);
                    if (!bytes.contains('\n'))
                        return;
                    const auto object = QJsonDocument::fromJson(bytes.trimmed()).object();
                    socket->setProperty("handled", true);
                    dispatch(object.value("command").toString());
                    socket->disconnectFromServer();
                };
                connect(socket, &QLocalSocket::readyRead, this, consume);
                if (socket->bytesAvailable())
                    QTimer::singleShot(0, socket, consume);
                QTimer::singleShot(2000, socket, [socket] { socket->disconnectFromServer(); });
            }
        });
        // Do not capture clipboard contents left over before this component started.
        if (!test)
            connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
                if (history.enabled && history.capture(QApplication::clipboard()->mimeData()))
                    refresh();
            });
        if (!test)
            registerKeys();
        return true;
    }
    void dispatch(const QString& command)
    {
        if (test && command != "clipboard" && command != "settings" && command != "stop")
            return;
        if (command == "stop" || (command == "stop-if-idle" && !desktopPresent())) {
            shutdown();
            return;
        }
        if (command == "clipboard" || command == "settings") {
            ++requests;
            refresh(true);
            if (isMinimized())
                showNormal();
            else
                show();
            raise();
            activateWindow();
            if (command == "settings")
                hotkeys->setFocus();
            else
                items->setFocus();
        } else if (command == "dolphin")
            launch("dolphin", {});
        else if (command == "desktop") {
            hide();
            desktop.toggle();
        } else if (command == "run") {
            bool accepted = false;
            const auto text = QInputDialog::getText(
                this, tr("Run application"), tr("Program and arguments:"), QLineEdit::Normal, {}, &accepted);
            if (!accepted || text.trimmed().isEmpty())
                return;
            const auto parts = QProcess::splitCommand(text);
            if (parts.isEmpty() || !QProcess::startDetached(parts.first(), parts.mid(1)))
                QMessageBox::warning(this, tr("Cannot start application"),
                    tr("Check the executable path and arguments. Shell operators belong in Konsole."));
        }
    }
    bool uiTest()
    {
        history.clear();
        refresh();
        bool ok = items->count() == 0 && !clearButton->isEnabled() && !copyButton->isEnabled()
            && !emptyHint->isHidden();
        QMimeData data;
        data.setText(QStringLiteral("中文 clipboard fixture"));
        history.capture(&data);
        refresh();
        ok &= clearButton->isEnabled() && emptyHint->isHidden() && items->count() == 1
            && items->item(0)->data(Qt::UserRole).toString() == data.text();
        hide();
        data.setText("hidden fixture");
        history.capture(&data);
        refresh();
        ok &= items->count() == 1; // Hidden capture does not rebuild the list.
        show();
        ok &= items->count() == 2; // Native reopening also refreshes without IPC.
        dispatch("settings");
        QCoreApplication::processEvents();
        ok &= items->count() == 2 && focusWidget() == hotkeys;
        dispatch("clipboard");
        ok &= focusWidget() == items;
        requests = 0;
        MSG session {};
        session.message = WM_QUERYENDSESSION;
        qintptr result = 0;
        ok &= nativeEventFilter({}, &session, &result) && result == TRUE;
        session.message = WM_ENDSESSION;
        session.wParam = FALSE;
        ok &= !nativeEventFilter({}, &session, &result);
        if (!start() || !start())
            return false;
        QLocalSocket socket;
        socket.connectToServer(pipe);
        if (!socket.waitForConnected(1000))
            return false;
        showMinimized();
        socket.write("{\"command\":\"clipboard\"}\n");
        socket.flush();
        QElapsedTimer timer;
        timer.start();
        while (requests == 0 && timer.elapsed() < 1000)
            QCoreApplication::processEvents();
        ok &= requests == 1 && !isMinimized();
        QLocalSocket oversized;
        oversized.connectToServer(pipe);
        if (!oversized.waitForConnected(1000))
            return false;
        oversized.write(QByteArray(2050, 'x'));
        oversized.flush();
        timer.restart();
        while (oversized.state() != QLocalSocket::UnconnectedState && timer.elapsed() < 1000)
            QCoreApplication::processEvents();
        ok &= oversized.state() == QLocalSocket::UnconnectedState && requests == 1;
        if (QCoreApplication::arguments().contains("--expect-chinese"))
            ok &= QFontMetrics(font()).inFontUcs4(0x5185) && windowTitle() == QStringLiteral("桌面工具与剪贴板")
                && findChild<QLabel*>("ShortcutPolicy")->text().contains(QStringLiteral("修饰键"))
                && findChild<QLabel*>("ClipboardPrivacy")->text().contains(QStringLiteral("内存"));
        const auto originalSettings = qgetenv("KDE_ON_WIN_SETTINGS");
        QTemporaryDir temporary;
        const QString blocked = temporary.filePath("blocked.ini");
        QDir().mkdir(blocked);
        qputenv("KDE_ON_WIN_SETTINGS", blocked.toUtf8());
        save("clipboardHistoryLimit", 12);
        ok &= preferenceError && QFileInfo(blocked).isDir();
        qputenv("KDE_ON_WIN_SETTINGS", originalSettings);
        shortcutStatus = "fixture shortcuts";
        save("clipboardHistoryLimit", 20);
        ok &= !preferenceError && status->text() == shortcutStatus;
        history.setEnabled(false);
        refresh();
        ok &= !clearButton->isEnabled() && !copyButton->isEnabled() && !emptyHint->isHidden()
            && emptyHint->text() == tr("Text history is disabled.");
        QLocalSocket stop;
        stop.connectToServer(pipe);
        if (!stop.waitForConnected(1000))
            return false;
        stop.write("{\"command\":\"stop\"}\n");
        stop.flush();
        timer.restart();
        while (!stopped && timer.elapsed() < 1000)
            QCoreApplication::processEvents();
        return ok && stopped && history.entries.isEmpty() && !server.isListening();
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QMainWindow::showEvent(event);
        refresh(true);
    }
    void closeEvent(QCloseEvent* event) override
    {
        hide();
        event->ignore();
    }
    bool nativeEventFilter(const QByteArray&, void* data, qintptr* result) override
    {
        auto* message = static_cast<MSG*>(data);
        // This RAM-only utility must never veto Windows logoff/shutdown.
        if (message->message == WM_QUERYENDSESSION) {
            *result = TRUE;
            return true;
        }
        if (message->message == WM_ENDSESSION && message->wParam) {
            shutdown();
            *result = 0;
            return true;
        }
        if (!message->hwnd && message->message == WM_HOTKEY) {
            const QStringList commands { "dolphin", "run", "desktop", "clipboard" };
            const int index = int(message->wParam) - hotkeyBase;
            if (index < 0 || index >= commands.size())
                return false;
            dispatch(commands[index]);
            *result = 0;
            return true;
        }
        return false;
    }

private:
    void shutdown()
    {
        stopped = true;
        history.clear();
        refresh();
        unregisterKeys();
        desktop.restore();
        server.close();
        if (!test)
            QCoreApplication::exit(0);
    }
    void save(const QString& key, const QVariant& value)
    {
        QSettings prefs(appearanceSettingsPath(), QSettings::IniFormat);
        prefs.setValue(key, value);
        prefs.sync();
        preferenceError = prefs.status() != QSettings::NoError;
        updateStatus();
    }
    void updateStatus()
    {
        status->setText(preferenceError
                ? tr("Settings could not be read or saved. Changes may last only for this run.")
                : shortcutStatus);
    }
    void launch(const QString& role, const QStringList& args)
    {
        if (!QProcess::startDetached(
                QCoreApplication::applicationDirPath() + "/plasma-kde-launcher.exe", QStringList { role } + args))
            QMessageBox::warning(
                this, tr("Cannot start application"), tr("The KDE launcher is missing or could not be started."));
    }
    void refresh(bool force = false)
    {
        if (!force && !isVisible())
            return;
        emptyHint->setText(history.enabled
                ? tr("No text history yet. Copy text to add it here.")
                : tr("Text history is disabled."));
        emptyHint->setVisible(history.entries.isEmpty());
        clearButton->setEnabled(!history.entries.isEmpty());
        const QString selected = items->currentItem() ? items->currentItem()->data(Qt::UserRole).toString() : QString();
        items->clear();
        for (const auto& text : history.entries) {
            auto* item = new QListWidgetItem(text.left(512).simplified().left(160), items);
            item->setData(Qt::UserRole, text);
            if (text == selected)
                items->setCurrentItem(item);
        }
        if (!items->currentItem() && items->count())
            items->setCurrentRow(0);
    }
    void select()
    {
        if (!items->currentItem())
            return;
        if (!test) {
            auto* data = new QMimeData;
            data->setText(items->currentItem()->data(Qt::UserRole).toString());
            data->setData("application/x-kde-on-win-history", "1");
            QApplication::clipboard()->setMimeData(data);
        }
        hide();
    }
    void unregisterKeys()
    {
        for (int id : registered)
            UnregisterHotKey(nullptr, id);
        registered.clear();
    }
    void registerKeys()
    {
        unregisterKeys();
        const UINT flags = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT | (hotkeys->currentIndex() ? MOD_SHIFT : 0);
        QStringList failed;
        const QString keys = "ERDV";
        for (int i = 0; i < 4; ++i) {
            if (RegisterHotKey(nullptr, hotkeyBase + i, flags, keys[i].unicode()))
                registered.append(hotkeyBase + i);
            else
                failed.append(keys[i]);
        }
        shortcutStatus = failed.isEmpty()
                ? tr("Shortcuts registered.")
                : tr("Shortcuts unavailable: %1. Choose the alternate modifiers or use the menu.")
                      .arg(failed.join('/'));
        updateStatus();
    }
    QString pipe = channel();
    bool preferenceError = false;
    bool stopped = false;
    int requests = 0;
    bool test = false;
    ClipboardHistory history;
    DesktopToggle desktop;
    QLocalServer server;
    QString shortcutStatus;
    QLabel* emptyHint = nullptr;
    QPushButton* clearButton = nullptr;
    QPushButton* copyButton = nullptr;
    QListWidget* items;
    QComboBox* hotkeys;
    QLabel* status;
    QList<int> registered;
};
static bool selfTest()
{
    ClipboardHistory history;
    QMimeData data;
    data.setText("alpha");
    bool ok = history.capture(&data) && !history.capture(&data);
    data.setText(QStringLiteral("中文"));
    ok &= history.capture(&data);
    history.setLimit(1);
    ok &= history.entries.size() == 1 && history.entries.first() == QStringLiteral("中文");
    data.setData("application/x-kde-passwordManagerHint", "secret");
    ok &= !history.capture(&data);
    data.clear();
    data.setText("secret");
    data.setData("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\"", QByteArray(4, 0));
    ok &= !history.capture(&data);
    data.clear();
    data.setText("excluded");
    data.setData("application/x-qt-windows-mime;value=\"ExcludeClipboardContentFromMonitorProcessing\"", "");
    ok &= !history.capture(&data);
    data.clear();
    data.setText("selection");
    data.setData("application/x-kde-on-win-history", "1");
    ok &= !history.capture(&data);
    data.clear();
    data.setText("allowed");
    QByteArray inclusion(4, 0);
    inclusion[0] = 1;
    data.setData("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\"", inclusion);
    ok &= history.capture(&data);
    data.clear();
    data.setUrls({ QUrl::fromLocalFile(QStringLiteral("C:/private/path")) });
    ok &= !history.capture(&data);
    data.setText("file:///C:/private/path");
    ok &= !history.capture(&data);
    data.clear();
    data.setText("https://example.invalid/text");
    data.setUrls({ QUrl("https://example.invalid/text") });
    ok &= history.capture(&data);
    data.clear();
    data.setText("private fixture");
    data.setData("application/x-kde-passwordManagerHint", QByteArray("secret\0\0", 8));
    ok &= !history.capture(&data);
    data.clear();
    data.setText(QString(65537, 'x'));
    ok &= !history.capture(&data);
    history.setEnabled(false);
    data.setText("disabled");
    ok &= !history.capture(&data) && history.entries.isEmpty();
    history.clear();
    ok &= !history.capture(nullptr);
    ok &= toolsSessionEnded(true, true, false, 1000) && toolsSessionEnded(true, false, false, 60000)
        && !toolsSessionEnded(true, false, false, 1000) && !toolsSessionEnded(false, true, false, 100000)
        && !toolsSessionEnded(true, true, true, 100000);
    return ok;
}
int main(int argc, char** argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    installProjectTranslations(&app);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("plasma-desktop-tools");
    app.setDesktopFileName("org.nixbridge.desktoptools");
    const auto args = app.arguments();
    if (args.contains("--ui-smoke-test")) {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
    }
    if (args.contains("--self-test")) {
        const bool ok = selfTest();
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": bounded RAM history, Unicode, deduplication, privacy markers and selection loop "
                               "suppression; no system clipboard access\n";
        return ok ? 0 : 1;
    }
    if (args.contains("--ui-smoke-test")) {
        DesktopTools window(true);
        window.show();
        QTimer::singleShot(400, &window, [&] {
            const bool ok = window.uiTest() && window.findChild<QWidget*>("PlasmaBreezeTitleBar");
            window.grab().save("desktop-tools-preview.png");
            QTextStream(stdout)
                << (ok ? "PASS" : "FAIL")
                << ": isolated desktop-tools UI; private IPC and UI; no hotkeys or system clipboard access\n";
            app.exit(ok ? 0 : 1);
        });
        return app.exec();
    }
    const QString option = args.value(1, "--clipboard");
    const QMap<QString, QString> commands { { "--daemon", "daemon" }, { "--clipboard", "clipboard" },
        { "--settings", "settings" }, { "--run", "run" }, { "--desktop", "desktop" }, { "--stop", "stop" },
        { "--stop-if-idle", "stop-if-idle" } };
    if (args.size() > 2 || !commands.contains(option))
        return 2;
    const auto name = channel().toStdWString();
    WindowsInstanceLock instance(name.c_str());
    if (!instance.valid())
        return 2;
    if (instance.duplicate()) {
        QLocalSocket socket;
        socket.connectToServer(channel());
        if (!socket.waitForConnected(1500))
            return 1;
        ULONG serverPid = 0;
        if ((option == "--clipboard" || option == "--settings" || option == "--run")
            && GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket.socketDescriptor()), &serverPid))
            AllowSetForegroundWindow(serverPid);
        const auto request
            = QJsonDocument(QJsonObject { { "command", commands.value(option) } }).toJson(QJsonDocument::Compact)
            + '\n';
        if (socket.write(request) != request.size())
            return 1;
        return socket.bytesToWrite() == 0 || socket.waitForBytesWritten(1000) ? 0 : 1;
    }
    if (option == "--stop" || option == "--stop-if-idle")
        return 0;
    DesktopTools window;
    if (!window.start())
        return 1;
    QTimer lifetime;
    QElapsedTimer uptime;
    uptime.start();
    bool seenDesktop = false;
    if (option == "--daemon") {
        QObject::connect(&lifetime, &QTimer::timeout, &window, [&] {
            const bool alive = desktopPresent();
            seenDesktop |= alive;
            if (toolsSessionEnded(true, seenDesktop, alive, uptime.elapsed()))
                app.exit(0);
        });
        lifetime.start(5000);
    }
    if (option != "--daemon")
        QTimer::singleShot(0, &window, [&] { window.dispatch(commands.value(option)); });
    return app.exec();
}
#include "desktop-tools.moc"
