// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowsiconmap.h"
#include <QGuiApplication>
#include <QTextStream>
#include <KIconTheme>
#include <KIconLoader>
int main(int argc, char **argv)
{
    KIconTheme::initTheme();
    QGuiApplication app(argc, argv);
    bool ok = true;
    QTextStream out(stdout);
    for (const auto &program : {"kde:konsole", "kde:process-monitor", "kde:dolphin", "notepad.exe", "calc.exe", "explorer.exe", "mspaint.exe", "cmd.exe",
         "powershell.exe", "taskmgr.exe", "regedit.exe", "control.exe", "snippingtool.exe", "mstsc.exe",
         "magnify.exe", "osk.exe", "charmap.exe", "wmplayer.exe", "msedge.exe", "eventvwr.msc",
         "services.msc", "devmgmt.msc", "apps:Microsoft.WindowsCalculator_8wekyb3d8bbwe!App",
         "apps:Microsoft.Windows.Photos_8wekyb3d8bbwe!App", "apps:Microsoft.WindowsStore_8wekyb3d8bbwe!App"}) {
        const QString icon = breezeIconForWindowsProgram(QString::fromLatin1(program));
        const QString path = KIconLoader::global()->iconPath(icon, -48, true);
        const bool found = !icon.isEmpty() && path.startsWith(QLatin1String(":/icons/breeze/"));
        out << (found ? "PASS " : "FAIL ") << program << " -> " << icon << " [" << path << "]\n";
        ok &= found;
    }
    ok &= windowsProgramIconCandidates(QStringLiteral("third-party-app.exe")).isEmpty();
    return ok ? 0 : 1;
}
