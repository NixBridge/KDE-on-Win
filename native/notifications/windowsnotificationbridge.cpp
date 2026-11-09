// SPDX-License-Identifier: GPL-3.0-or-later
#include "../apps/localization.h"
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QSet>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QProcess>
#include <QTextStream>
#include <KIconTheme>
#include <windows.h>
#include <appmodel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include "notificationrelay.h"
#undef interface

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::UI::Notifications::Management;
static QString text(const winrt::hstring &value) { return QString::fromWCharArray(value.c_str(), int(value.size())); }
struct Toast { uint id; QString app, appId, title, body; };

class Bridge final : public QWidget
{
    Q_OBJECT
public:
    Bridge() : relay(this)
    {
        setWindowTitle(tr("Windows notifications in KDE")); resize(520, 230);
        auto *layout = new QVBoxLayout(this);
        status = new QLabel(tr("Allow this application to read Windows notifications and display them in Plasma. Notification text stays on this computer."), this);
        status->setWordWrap(true); layout->addWidget(status);
        auto *enable = new QPushButton(tr("Enable notification access"), this); layout->addWidget(enable);
        auto *test = new QPushButton(tr("Send Windows test notification"), this); layout->addWidget(test);
        auto *repository=new QPushButton(QObject::tr("Project repository"),this);layout->addWidget(repository);
        connect(repository,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/NixBridge/KDE-on-Win"));});

        connect(test, &QPushButton::clicked, this, [this] {
            try {
                auto xml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
                auto nodes = xml.GetElementsByTagName(L"text");
                nodes.Item(0).AppendChild(xml.CreateTextNode(L"KDE Windows Notifications"));
                nodes.Item(1).AppendChild(xml.CreateTextNode(L"Windows notification bridge test"));
                ToastNotificationManager::CreateToastNotifier().Show(ToastNotification(xml));
            } catch (const winrt::hresult_error &error) { status->setText(text(error.message())); }
        });
        connect(enable, &QPushButton::clicked, this, [this] { requestAccess(); });
        connect(&timer, &QTimer::timeout, this, [this] { poll(); }); timer.setInterval(1500);
        connect(&relay, &NotificationRelay::delivered, this, [this](const QString &key) { pending.remove(key); });
        connect(&relay, &NotificationRelay::failed, this, [this](const QString &key, const QString &error) { pending.remove(key); forwarded.remove(key); log(QStringLiteral("Delivery error: ") + error); });
        // The listener API does not expose another application's activation arguments.
        // Do not invent actions, remove the Windows original, or acknowledge it remotely.
        QTimer::singleShot(0, this, [this] {
            try {
                if (UserNotificationListener::Current().GetAccessStatus() == UserNotificationListenerAccessStatus::Allowed) { hide(); timer.start(); poll(); }
                else show();
            } catch (const winrt::hresult_error &error) { status->setText(tr("Install the notification package first. Windows error: %1").arg(text(error.message()))); log(text(error.message())); show(); }
        });
    }
    ~Bridge() override { timer.stop(); if (worker) { worker->quit(); worker->wait(); } }
public Q_SLOTS:
    void showSettings() { show(); raise(); activateWindow(); }
public:
    static void log(const QString &message)
    {
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-win-session");
        QDir().mkpath(directory); QFile file(directory + QStringLiteral("/notifications.log"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + message + QLatin1Char('\n')).toUtf8());
    }
private:
    void requestAccess()
    {
        try {
            auto operation = UserNotificationListener::Current().RequestAccessAsync();
            operation.Completed([this](const auto &result, winrt::Windows::Foundation::AsyncStatus) {
                bool allowed = false; QString error;
                try { allowed = result.GetResults() == UserNotificationListenerAccessStatus::Allowed; }
                catch (const winrt::hresult_error &failure) { error = text(failure.message()); }
                QMetaObject::invokeMethod(this, [this, allowed, error] {
                    if (allowed) { log(QStringLiteral("Notification access granted")); hide(); timer.start(); poll(); }
                    else { status->setText(tr("Windows notification access was not granted. %1").arg(error)); show(); }
                }, Qt::QueuedConnection);
            });
        } catch (const winrt::hresult_error &error) { status->setText(text(error.message())); log(text(error.message())); }
    }
    void poll()
    {
        if (worker) return;
        // No notification permission prompts or blocking WinRT work on Plasma's UI thread.
        worker = QThread::create([this] {
            QList<Toast> snapshot; QString error; bool allowed = false;
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                auto listener = UserNotificationListener::Current();
                allowed = listener.GetAccessStatus() == UserNotificationListenerAccessStatus::Allowed;
                if (allowed) {
                    for (const auto &notification : listener.GetNotificationsAsync(NotificationKinds::Toast).get()) {
                        Toast toast{}; toast.id = notification.Id();
                        try { toast.app = text(notification.AppInfo().DisplayInfo().DisplayName()); toast.appId = text(notification.AppInfo().AppUserModelId()); } catch (...) { toast.app = QStringLiteral("Windows"); }
                        auto binding = notification.Notification().Visual().GetBinding(KnownNotificationBindings::ToastGeneric());
                        if (!binding) continue;
                        QStringList lines;
                        for (const auto &element : binding.GetTextElements()) lines.append(text(element.Text()));
                        if (lines.isEmpty()) continue;
                        toast.title = lines.takeFirst(); toast.body = lines.join(QLatin1Char('\n')); snapshot.append(toast);
                    }
                }
            } catch (const winrt::hresult_error &failure) { error = text(failure.message()); }
            QMetaObject::invokeMethod(this, [this, snapshot, allowed, error] { apply(snapshot, allowed, error); }, Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, this, [this] { worker->deleteLater(); worker = nullptr; }); worker->start();
    }
    void apply(const QList<Toast> &snapshot, bool allowed, const QString &error)
    {
        if (!error.isEmpty()) { if (lastError != error) log(error); lastError = error; status->setText(tr("Notification listener error: %1").arg(error)); show(); return; }
        if (!allowed) { timer.stop(); status->setText(tr("Notification permission was revoked. Enable access to resume.")); show(); return; }
        const auto registered = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.freedesktop.Notifications"));
        if (!registered.isValid() || !registered.value()) return; // Do not consume history before Plasma is ready.
        QSet<QString> current;
        for (const auto &toast : snapshot) {
            const QString key = QStringLiteral("toast:%1:%2").arg(toast.appId).arg(toast.id); current.insert(key);
            const QString signature = toast.title + QChar(0) + toast.body;
            if (!primed) { forwarded.insert(key, signature); continue; } // Never replay old private history.
            if (forwarded.value(key) == signature || pending.contains(key)) continue;
            forwarded.insert(key, signature); pending.insert(key);
            relay.publish(key, toast.app, toast.title, toast.body);
        }
        for (const QString &key : forwarded.keys()) if (!current.contains(key)) { relay.withdraw(key); forwarded.remove(key); pending.remove(key); }
        if (!primed) log(QStringLiteral("Listener ready; existing notification history skipped"));
        primed = true;
    }
    NotificationRelay relay;
    QLabel *status = nullptr;
    QTimer timer;
    QThread *worker = nullptr;
    bool primed = false;
    QString lastError;
    QHash<QString, QString> forwarded;
    QSet<QString> pending;
};

int main(int argc, char **argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")) qputenv("DBUS_SESSION_BUS_ADDRESS", "tcp:host=127.0.0.1,port=12443");
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    KIconTheme::initTheme();
    QApplication app(argc, argv); installProjectTranslations(&app); app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains(QStringLiteral("--check-platform"))) {
        const bool available = winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(L"Windows.UI.Notifications.Management.UserNotificationListener");
        UINT32 length = 0; const LONG package = GetCurrentPackageFullName(&length, nullptr);
        QTextStream(stdout) << (available ? "PASS" : "FAIL") << ": Windows notification listener API available; package identity=" << (package == ERROR_INSUFFICIENT_BUFFER ? "yes" : "no") << "; no notification access requested\n";
        return available ? 0 : 1;
    }
    app.setApplicationName(QStringLiteral("KDE Windows Notifications"));
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.nixbridge.WindowsNotifications"))) {
        auto request = QDBusMessage::createMethodCall(QStringLiteral("org.nixbridge.WindowsNotifications"), QStringLiteral("/Bridge"), QString(), QStringLiteral("showSettings"));
        bus.call(request, QDBus::Block, 2000);
        return 0;
    }
    Bridge window;
    bus.registerObject(QStringLiteral("/Bridge"), &window, QDBusConnection::ExportAllSlots);
    return app.exec();
}
#include "windowsnotificationbridge.moc"
