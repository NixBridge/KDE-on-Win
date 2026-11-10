// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
#include <QCoreApplication>
#include <QProcess>
#include <QObject>
#include <QStringList>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

struct DesktopAction {
    QString id, text, icon;
};
inline QList<DesktopAction> desktopPlaces()
{
    return { { "home", QObject::tr("Home"), "user-home" }, { "computer", QObject::tr("This PC"), "computer" },
        { "downloads", QObject::tr("Downloads"), "folder-download" },
        { "documents", QObject::tr("Documents"), "folder-documents" },
        { "pictures", QObject::tr("Pictures"), "folder-pictures" },
        { "desktop", QObject::tr("Desktop"), "user-desktop" },
        { "recycle", QObject::tr("Recycle Bin (Windows)"), "user-trash" } };
}
inline QList<DesktopAction> desktopQuickActions()
{
    return { { "windows-settings", QObject::tr("Windows Settings"), "preferences-system" },
        { "display-settings", QObject::tr("Display Settings"), "preferences-desktop-display" },
        { "process-monitor", QObject::tr("Task Manager"), "utilities-system-monitor" },
        { "dolphin", QObject::tr("Dolphin"), "system-file-manager" },
        { "konsole", QObject::tr("Konsole"), "utilities-terminal" },
        { "system-information", QObject::tr("System Information"), "computer" },
        { "logs", QObject::tr("KDE-on-Win logs"), "folder" } };
}
struct DesktopActionPlan {
    QString program;
    QStringList arguments;
    QString error;
    bool shell = false;
    bool createLogs = false;
};
inline DesktopActionPlan desktopActionPlan(
    const QString& id, const QString& bin = QCoreApplication::applicationDirPath())
{
    const KNOWNFOLDERID* folder = id == "home" ? &FOLDERID_Profile
        : id == "downloads"                    ? &FOLDERID_Downloads
        : id == "documents"                    ? &FOLDERID_Documents
        : id == "pictures"                     ? &FOLDERID_Pictures
        : id == "desktop"                      ? &FOLDERID_Desktop
                                               : nullptr;
    if (folder) {
        PWSTR path = nullptr;
        const auto hr = SHGetKnownFolderPath(*folder, KF_FLAG_DONT_VERIFY, nullptr, &path);
        QString value = SUCCEEDED(hr) && path ? QString::fromWCharArray(path) : QString();
        CoTaskMemFree(path);
        if (value.isEmpty())
            return { {}, {}, QObject::tr("Windows could not resolve this location."), false };
        return { bin + "/dolphin.exe", { value }, {}, false };
    }
    if (id == "computer" || id == "recycle") {
        wchar_t system[32768] {};
        GetWindowsDirectoryW(system, 32768);
        return { QString::fromWCharArray(system) + "/explorer.exe",
            { id == "computer" ? "shell:MyComputerFolder" : "shell:RecycleBinFolder" }, {}, false };
    }
    if (id == "windows-settings")
        return { "ms-settings:", {}, {}, true };
    if (id == "display-settings")
        return { bin + "/plasma-system-settings.exe", { "--page=display" }, {}, false };
    if (id == "process-monitor")
        return { bin + "/plasma-process-monitor.exe", {}, {}, false };
    if (id == "dolphin")
        return { bin + "/dolphin.exe", {}, {}, false };
    if (id == "konsole")
        return { bin + "/konsole.exe", { "--separate" }, {}, false };
    if (id == "logs")
        return { bin + "/dolphin.exe", { QFileInfo(appearanceSettingsPath()).absolutePath() + "/logs" }, {}, false,
            true };
    if (id == "system-information") {
        wchar_t system[32768] {};
        GetSystemDirectoryW(system, 32768);
        return { QString::fromWCharArray(system) + "/msinfo32.exe", {}, {}, false };
    }
    return { {}, {}, QObject::tr("Unknown desktop action."), false };
}
inline QString startDesktopAction(const QString& id)
{
    const auto plan = desktopActionPlan(id);
    if (!plan.error.isEmpty())
        return plan.error;
    if (plan.shell) {
        const auto target = plan.program.toStdWString();
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return result > 32 ? QString() : QObject::tr("Windows could not open this action (error %1).").arg(result);
    }
    if (!QFileInfo::exists(plan.program))
        return QObject::tr("The required component is missing: %1").arg(QFileInfo(plan.program).fileName());
    if (plan.createLogs && !QDir().mkpath(plan.arguments.first()))
        return QObject::tr("The log directory could not be created.");
    return QProcess::startDetached(plan.program, plan.arguments) ? QString()
                                                                 : QObject::tr("The application could not be started.");
}
