// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
#include "windowsiconmap.h"
#include <QDir>
#include <QFileInfo>
#include <KIconLoader>

QStringList windowsProgramIconCandidates(const QString &program)
{
    const QString value = QDir::fromNativeSeparators(program).toLower();
    const QString name = QFileInfo(value).fileName();
    struct Entry { const char *program; const char *icons; };
    static const Entry entries[] = {
        {"notepad.exe", "accessories-text-editor;org.kde.kwrite;text-plain"},
        {"calc.exe", "accessories-calculator;kcalc"},
        {"calculatorapp.exe", "accessories-calculator;kcalc"},
        {"explorer.exe", "system-file-manager;org.kde.dolphin;folder"},
        {"mspaint.exe", "kolourpaint;applications-graphics;draw-freehand"},
        {"cmd.exe", "utilities-terminal;org.kde.konsole"},
        {"powershell.exe", "utilities-terminal;org.kde.konsole"},
        {"pwsh.exe", "utilities-terminal;org.kde.konsole"},
        {"windowsterminal.exe", "utilities-terminal;org.kde.konsole"},
        {"wt.exe", "utilities-terminal;org.kde.konsole"},
        {"taskmgr.exe", "utilities-system-monitor;org.kde.plasma.systemmonitor"},
        {"regedit.exe", "preferences-system;document-properties"},
        {"control.exe", "preferences-system;systemsettings"},
        {"systemsettings.exe", "preferences-system;systemsettings"},
        {"snippingtool.exe", "spectacle;accessories-screenshot"},
        {"mstsc.exe", "krdc;preferences-desktop-remote-desktop"},
        {"magnify.exe", "preferences-desktop-accessibility;zoom-in"},
        {"osk.exe", "input-keyboard;preferences-desktop-keyboard"},
        {"charmap.exe", "accessories-character-map;kcharselect"},
        {"wmplayer.exe", "multimedia-player;applications-multimedia"},
        {"msedge.exe", "internet-web-browser;web-browser"},
        {"eventvwr.msc", "utilities-log-viewer;document-open-recent"},
        {"services.msc", "preferences-system-services;preferences-system"},
        {"devmgmt.msc", "preferences-devices;computer"},
        {"compmgmt.msc", "preferences-system;computer"},
        {"mmc.exe", "preferences-system;systemsettings"},
    };
    for (const auto &entry : entries)
        if (name == QLatin1String(entry.program)) return QString::fromLatin1(entry.icons).split(';');
    struct App { const char *id; const char *icons; };
    static const App apps[] = {
        {"microsoft.windowscalculator", "accessories-calculator;kcalc"},
        {"microsoft.windowsnotepad", "accessories-text-editor;org.kde.kwrite"},
        {"microsoft.paint", "kolourpaint;applications-graphics"},
        {"microsoft.windows.photos", "org.kde.gwenview;image-viewer"},
        {"microsoft.windowscamera", "camera-photo;camera-web"},
        {"microsoft.windowssoundrecorder", "media-record;audio-input-microphone"},
        {"microsoft.microsoftstickynotes", "knotes;accessories-notes"},
        {"microsoft.windowsalarms", "preferences-system-time;clock"},
        {"microsoft.windowsstore", "plasmadiscover;system-software-install"},
        {"microsoft.windowsterminal", "utilities-terminal;org.kde.konsole"},
        {"microsoft.screenSketch", "spectacle;accessories-screenshot"},
        {"windows.immersivecontrolpanel", "preferences-system;systemsettings"},
        {"microsoft.sechealthui", "security-high;preferences-security"},
    };
    for (const auto &app : apps)
        if (value.contains(QString::fromLatin1(app.id).toLower())) return QString::fromLatin1(app.icons).split(';');
    return {};
}

QString breezeIconForWindowsProgram(const QString &program)
{
    for (const QString &candidate : windowsProgramIconCandidates(program))
        if (!KIconLoader::global()->iconPath(candidate, -48, true).isEmpty()) return candidate;
    return {};
}
