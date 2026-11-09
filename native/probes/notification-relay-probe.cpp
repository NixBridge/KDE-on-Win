// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <QDBusContext>
#include <QDBusMessage>
#include "notificationrelay.h"

class Server : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    int calls = 0, closed = 0;
    uint lastId = 0;
    bool hold = false;
    int timeout = 0;
    QDBusMessage held;
    QString body, summary;
    QStringList actions;
public Q_SLOTS:
    uint Notify(const QString &, uint replaces, const QString &, const QString &title, const QString &content,
                const QStringList &buttons, const QVariantMap &, int expiry)
    {
        ++calls; summary = title; body = content; actions = buttons; timeout = expiry;
        if(hold) {setDelayedReply(true);held=message();return 0;}
        lastId = replaces ? replaces : ++nextId;
        return lastId;
    }
    void CloseNotification(uint id) { ++closed; Q_EMIT NotificationClosed(id, 3); }
Q_SIGNALS:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &action);
private:
    uint nextId = 0;
};
static bool waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < 5000) { QCoreApplication::processEvents(); QThread::msleep(5); }
    return condition();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    QFile config(temp.filePath("session.conf")); config.open(QIODevice::WriteOnly);
    config.write("<busconfig><type>session</type><listen>tcp:host=127.0.0.1,port=0</listen><auth>ANONYMOUS</auth><allow_anonymous/><policy context=\"default\"><allow own=\"*\"/><allow send_destination=\"*\"/><allow receive_sender=\"*\"/></policy></busconfig>"); config.close();
    QProcess daemon; daemon.start(QCoreApplication::applicationDirPath()+"/dbus-daemon.exe", {"--config-file="+config.fileName(), "--nofork", "--print-address=1"});
    if (!daemon.waitForStarted(5000) || !daemon.waitForReadyRead(5000)) return 2;
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    auto serverBus = QDBusConnection::connectToBus(address, "notification-probe-server");
    auto clientBus = QDBusConnection::connectToBus(address, "notification-probe-client");
    Server server;
    if (!serverBus.registerService("org.freedesktop.Notifications") || !serverBus.registerObject("/org/freedesktop/Notifications", &server, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) return 3;
    bool ok = true; int delivered = 0, activated = 0, dismissed = 0;
    NotificationRelay relay(nullptr, clientBus);
    QObject::connect(&relay, &NotificationRelay::delivered, [&] { ++delivered; });
    QObject::connect(&relay, &NotificationRelay::activated, [&] { ++activated; });
    QObject::connect(&relay, &NotificationRelay::dismissed, [&] { ++dismissed; });
    relay.publish("test", "Windows test", "标题", "<b>plain</b>\n正文", "dialog-information", true);
    ok &= waitFor([&] { return delivered == 1; });
    ok &= server.body == "&lt;b&gt;plain&lt;/b&gt;<br/>正文" && server.actions.contains("default") && server.timeout == 8000;
    const uint first = server.lastId;
    relay.publish("test", "Windows test", "Updated", "replacement");
    ok &= waitFor([&] { return delivered == 2; }) && server.lastId == first;
    Q_EMIT server.ActionInvoked(first, "default");
    ok &= waitFor([&] { return activated == 1; });
    Q_EMIT server.NotificationClosed(first, 1);
    ok &= waitFor([&] { return dismissed == 1; });
    relay.publish("cancel", "Windows test", "cancel", "pending"); relay.withdraw("cancel");
    ok &= waitFor([&] { return server.closed >= 1; });
    relay.publish("latest", "Windows test", "old", "old"); relay.publish("latest", "Windows test", "new", "new");
    ok &= waitFor([&] { return server.summary == "new"; });
    relay.withdraw("latest");
    ok &= waitFor([&] { return server.closed >= 2; });
    server.hold = true;
    const int beforeCalls = server.calls;
    relay.publish("late", "Windows test", "late", "pending old owner");
    ok &= waitFor([&] { return server.calls > beforeCalls; });
    serverBus.unregisterService("org.freedesktop.Notifications");
    auto settle = [] { QElapsedTimer timer; timer.start(); while(timer.elapsed()<250) {QCoreApplication::processEvents();QThread::msleep(2);} };
    settle();
    ok &= serverBus.registerService("org.freedesktop.Notifications");
    settle(); server.hold = false;
    const int beforeDelivery = delivered;
    relay.publish("new-owner", "Windows test", "new owner", "fresh");
    ok &= waitFor([&] { return delivered > beforeDelivery; });
    const int beforeClose = server.closed;
    serverBus.send(server.held.createReply(QVariantList{server.lastId}));
    settle();
    ok &= server.closed == beforeClose;
    relay.withdraw("new-owner");
    ok &= waitFor([&] { return server.closed > beforeClose; });
    daemon.terminate(); if (!daemon.waitForFinished(2000)) { daemon.kill(); daemon.waitForFinished(2000); }
    QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": isolated D-Bus notification delivery, Unicode escaping, replacement, action, close, cancellation, queued update, timeout and stale replies across owner restart\n";
    return ok ? 0 : 1;
}
#include "notification-relay-probe.moc"
