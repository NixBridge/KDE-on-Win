// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSysInfo>
#include <QRegularExpression>
#include <windows.h>
#include <tlhelp32.h>
inline QString diagnosticFileVersion(const QString& path)
{
    const auto file = QDir::toNativeSeparators(path).toStdWString();
    DWORD unused = 0;
    const auto length = GetFileVersionInfoSizeW(file.c_str(), &unused);
    if (!length || length > 1024 * 1024)
        return {};
    QByteArray bytes(length, 0);
    if (!GetFileVersionInfoW(file.c_str(), 0, length, bytes.data()))
        return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&info), &size) || size < sizeof(*info))
        return {};
    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
}
inline QJsonObject diagnosticReport(const QString& bin, const QString& prefsPath, const QString& logs)
{
    QJsonObject report { { "format", 1 }, { "windows", QSysInfo::productVersion() },
        { "architecture", QSysInfo::currentCpuArchitecture() }, { "qt", QString::fromLatin1(qVersion()) },
        { "privacy", "No environment, command lines, clipboard, usernames or raw log messages included." } };
    QFile build(bin + "/data/kde-on-win/build-info.json");
    if (build.open(QIODevice::ReadOnly)) {
        const auto object = QJsonDocument::fromJson(build.read(4096)).object();
        const auto commit = object.value("commit").toString();
        if (QRegularExpression("^[0-9a-f]{40}$").match(commit).hasMatch())
            report.insert("commit", commit);
        const auto version = object.value("version").toString();
        if (QRegularExpression("^Fix[0-9]+$").match(version).hasMatch())
            report.insert("version", version);
        report.insert("sourceDirty", object.value("dirty").toBool());
    }
    QJsonArray components;
    for (const auto name : { "plasmashell.exe", "dolphin.exe", "konsole.exe", "plasma-desktop-tools.exe",
             "KF6WindowSystem.dll", "KF6ConfigCore.dll", "Qt6Core.dll" }) {
        const auto path = bin + "/" + name;
        components.append(QJsonObject { { "name", name }, { "present", QFileInfo::exists(path) },
            { "fileVersion", diagnosticFileVersion(path) },
            { "loadedInExporter", GetModuleHandleW(QString::fromLatin1(name).toStdWString().c_str()) != nullptr } });
    }
    report.insert("components", components);
    QJsonObject running;
    const auto expected = QDir::cleanPath(QDir::fromNativeSeparators(bin));
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry {};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
            do {
                const auto name = QString::fromWCharArray(entry.szExeFile).toLower();
                if (!QStringList {
                        "plasmashell.exe", "kded6.exe", "trayhost.exe", "plasma-desktop-tools.exe", "dolphin.exe" }
                        .contains(name))
                    continue;
                DWORD other = 0;
                if (!ProcessIdToSessionId(entry.th32ProcessID, &other) || other != session)
                    continue;
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (process) {
                    wchar_t image[32768] {};
                    DWORD length = 32768;
                    if (QueryFullProcessImageNameW(process, 0, image, &length)
                        && QFileInfo(QString::fromWCharArray(image, length))
                                .absolutePath()
                                .compare(expected, Qt::CaseInsensitive)
                            == 0)
                        running.insert(name, running.value(name).toInt() + 1);
                    CloseHandle(process);
                }
            } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
    }
    report.insert("currentSessionRuntimeProcesses", running);
    QSettings prefs(prefsPath, QSettings::IniFormat);
    QJsonObject configuration;
    for (const auto key : { "dark", "followWindowsTheme", "decorateWindows", "clipboardEnabled", "shortcutExtraShift",
             "pendingPersonalization", "pendingAppearance" })
        configuration.insert(key, prefs.value(key, false).toBool());
    configuration.insert("fontSize", qBound(8, prefs.value("fontSize", 10).toInt(), 20));
    configuration.insert("clipboardHistoryLimit", qBound(1, prefs.value("clipboardHistoryLimit", 20).toInt(), 200));
    configuration.insert("personalizationErrorPresent", !prefs.value("personalizationError").toString().isEmpty());
    report.insert("configuration", configuration);
    QJsonArray logSummary;
    for (const auto name :
        { "session-shell.log", "dbus-daemon.exe.log", "kded6.exe.log", "trayhost.exe.log", "personalization.log" }) {
        QFile log(QString::fromLatin1(name) == "personalization.log"
                ? QFileInfo(prefsPath).absolutePath() + "/personalization.log"
                : logs + "/" + name);
        if (!log.open(QIODevice::ReadOnly))
            continue;
        const auto size = log.size();
        log.seek(qMax<qint64>(0, size - 65536));
        const auto tail = log.read(65536).toLower();
        logSummary.append(QJsonObject { { "name", name }, { "bytes", static_cast<double>(size) },
            { "tailErrorMarkers", tail.count("error") + tail.count("failed") }, { "rawTextIncluded", false } });
    }
    report.insert("logs", logSummary);
    return report;
}
