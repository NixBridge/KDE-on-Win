// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QHash>
#include <QStandardPaths>
#include <KSharedConfig>
#include <KConfigGroup>
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.value(1) == QStringLiteral("--register-mime-defaults")
        || args.value(1) == QStringLiteral("--restore-mime-defaults")) {
        const bool restore = args.value(1) == QStringLiteral("--restore-mime-defaults");
        auto config = KSharedConfig::openConfig(
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/mimeapps.list",
            KConfig::SimpleConfig);
        const QString state
            = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/plasma-win-session";
        if (!QDir().mkpath(state))
            return 1;
        auto backup = KSharedConfig::openConfig(state + "/mime-defaults-backup.ini", KConfig::SimpleConfig);
        KConfigGroup defaults(config, "Default Applications"), original(backup, "Original"), existed(backup, "Existed"),
            written(backup, "Written");
        for (const char* mime : { "inode/directory", "application/zip", "application/x-7z-compressed",
                 "application/x-tar", "application/x-compressed-tar", "application/x-bzip-compressed-tar",
                 "application/x-xz-compressed-tar", "application/x-zstd-compressed-tar" }) {
            const QString desired = QString::fromLatin1(mime) == "inode/directory"
                ? QStringLiteral("org.kde.dolphin.desktop;")
                : QStringLiteral("org.nixbridge.archive.desktop;");
            if (restore) {
                if (!written.hasKey(mime) || defaults.readEntry(mime, QString()) != written.readEntry(mime, QString()))
                    continue;
                if (existed.readEntry(mime, false))
                    defaults.writeEntry(mime, original.readEntry(mime, QString()));
                else
                    defaults.deleteEntry(mime);
            } else {
                if (!written.hasKey(mime)) {
                    existed.writeEntry(mime, defaults.hasKey(mime));
                    original.writeEntry(mime, defaults.readEntry(mime, QString()));
                }
                written.writeEntry(mime, desired);
                if (!backup->sync())
                    return 1;
                defaults.writeEntry(mime, desired);
            }
        }
        return config->sync() ? 0 : 1;
    }
    const QHash<QString, QString> programs { { "archive", "plasma-archive-manager.exe" }, { "konsole", "konsole.exe" },
        { "dolphin", "dolphin.exe" }, { "process-monitor", "plasma-process-monitor.exe" },
        { "text-editor", "plasma-text-editor.exe" }, { "system-settings", "plasma-system-settings.exe" } };
    const QString program = programs.value(args.value(1));
    if (program.isEmpty())
        return 2;
    // Scope appearance to these child programs, not every Windows application.
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    QStringList childArgs = args.mid(2);
    if (args.value(1) == QStringLiteral("konsole") && !childArgs.contains(QStringLiteral("--separate")))
        childArgs.prepend(QStringLiteral("--separate"));
    return QProcess::startDetached(QDir(app.applicationDirPath()).filePath(program), childArgs) ? 0 : 1;
}
