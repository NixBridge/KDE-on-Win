// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include "personalization.h"
#include "appearance-config.h"
#include "localization.h"
#include <QStandardPaths>
int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"))
        qputenv("DBUS_SESSION_BUS_ADDRESS", "tcp:host=127.0.0.1,port=12443");
    QCoreApplication app(argc, argv);
    QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
    if (app.arguments().contains(QStringLiteral("--print-script"))) {
        QTextStream(stdout) << personalizationScript(settings);
        return 0;
    }
    if (!settings.value(QStringLiteral("pendingPersonalization"), false).toBool())
        return 0;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.nixbridge.Personalization")))
        return 1;
    QTimer timer;
    auto log = [&](const QString& message) {
        QFile file(QFileInfo(settings.fileName()).absolutePath() + QStringLiteral("/personalization.log"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Append))
            file.write(
                (QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + message + QLatin1Char('\n'))
                    .toUtf8());
    };
    QString lastError, appearanceRevision;
    timer.setInterval(1500);
    int attempts = 0;
    bool busy = false;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] {
        if (busy)
            return;
        if (++attempts > 80) {
            settings.setValue(
                "personalizationError", QStringLiteral("Plasma did not become ready; preferences remain pending"));
            settings.sync();
            log(QStringLiteral("Plasma did not become ready; preferences remain pending"));
            QTextStream(stderr) << "Plasma did not become ready; preferences remain pending.\n";
            app.exit(1);
            return;
        }
        settings.sync();
        const auto revision = settings.value(QStringLiteral("personalizationRevision")).toString();
        if (settings.value("pendingAppearance", false).toBool() && appearanceRevision != revision) {
            const QString scheme = app.applicationDirPath() + "/data/color-schemes/"
                + (settings.value("dark", false).toBool() ? "BreezeDark.colors" : "BreezeLight.colors");
            const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
            if (!writeAppearanceConfig(scheme, config + "/kdeglobals", config + "/plasmarc",
                    QFont(settings.value("fontFamily", defaultProjectFontFamily()).toString(),
                        qBound(8, settings.value("fontSize", 10).toInt(), 20)),
                    QColor(settings.value("accentColor").toString()),
                    QFileInfo(settings.fileName()).absolutePath() + "/current.colors")) {
                settings.setValue(
                    "personalizationError", QStringLiteral("Cannot save global appearance configuration"));
                settings.sync();
                log(QStringLiteral("Cannot save global appearance configuration"));
                return;
            }
            appearanceRevision = revision;
        }

        auto request = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
            QStringLiteral("/PlasmaShell"), QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("evaluateScript"));
        request.setAutoStartService(false);
        request << personalizationScript(settings);
        busy = true;
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), &app);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &app, [&, watcher, revision] {
            QDBusPendingReply<QString> reply = *watcher;
            watcher->deleteLater();
            busy = false;
            if (reply.isError() && reply.error().message() != lastError) {
                lastError = reply.error().message();
                log(lastError);
            }
            if (!reply.isError() && reply.value().contains(QStringLiteral("PERSONALIZATION_APPLIED"))) {
                settings.sync();
                if (settings.value(QStringLiteral("personalizationRevision")).toString() != revision)
                    return;
                settings.remove("personalizationError");
                settings.setValue(QStringLiteral("pendingPersonalization"), false);
                settings.setValue(QStringLiteral("pendingPanel"), false);
                settings.setValue(QStringLiteral("pendingWallpaper"), false);
                settings.setValue(QStringLiteral("pendingAppearance"), false);
                settings.sync();
                QTextStream(stdout) << reply.value();
                log(QStringLiteral("Personalization applied"));
                app.quit();
            }
        });
    });
    timer.start();
    return app.exec();
}
