// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QHash>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    const QHash<QString, QString> programs{{"konsole", "konsole.exe"}, {"dolphin", "dolphin.exe"}, {"process-monitor", "plasma-process-monitor.exe"}, {"text-editor", "plasma-text-editor.exe"}, {"system-settings", "plasma-system-settings.exe"}};
    const QString program = programs.value(args.value(1));
    if (program.isEmpty()) return 2;
    // Scope appearance to these child programs, not every Windows application.
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    QStringList childArgs=args.mid(2);
    if(args.value(1)==QStringLiteral("konsole") && !childArgs.contains(QStringLiteral("--separate")))childArgs.prepend(QStringLiteral("--separate"));
    return QProcess::startDetached(QDir(app.applicationDirPath()).filePath(program), childArgs) ? 0 : 1;
}
