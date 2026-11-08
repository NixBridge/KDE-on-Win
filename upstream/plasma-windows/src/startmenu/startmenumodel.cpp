/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "startmenumodel.h"
#include "windowsshortcut.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <KDesktopFile>
#include <QCollator>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <QTimer>
#include <QSettings>

#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QtQml/qqml.h>

StartMenuModel::StartMenuModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadCategories();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QSettings settings(base + QStringLiteral("/plasma-win-session/appearance.ini"), QSettings::IniFormat);
        const int pixels = qBound(24, settings.value(QStringLiteral("menuIconSize"), 40).toInt(), 72);
        const int columns = qBound(2, settings.value(QStringLiteral("menuColumns"), 3).toInt(), 5);
        if (pixels != m_iconPixels || columns != m_columns) {
            m_iconPixels = pixels;
            m_columns = columns;
            Q_EMIT appearanceChanged();
        }
        const QJsonArray previous = m_categories;
        loadCategories();
        if (m_categories != previous)
            Q_EMIT directoryChanged();
    });
    timer->start(1000);
}

int StartMenuModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : m_entries.size(); }

QVariant StartMenuModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry& e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case IconRole:
        return e.icon;
    case ExecRole:
        return e.exec;
    case IsDirRole:
        return e.isDir;
    case PathRole:
        return e.path;
    case LinkPathRole:
        return e.linkPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> StartMenuModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { IconRole, "icon" },
        { ExecRole, "exec" },
        { IsDirRole, "isDir" },
        { PathRole, "path" },
        { LinkPathRole, "linkPath" },
    };
}

QString StartMenuModel::directory() const { return m_directory; }
QString StartMenuModel::directoryName() const
{
    if (m_isRoot)
        return tr("All Apps");
    if (m_directory.startsWith(QLatin1String("cat:")))
        return m_directory.mid(4);
    QString path = m_directory;
    if (path.startsWith(QLatin1String("merged:")))
        path = QJsonDocument::fromJson(path.mid(7).toUtf8()).array().toVariantList().value(0).toString();
    SHFILEINFOW info {};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &info, sizeof(info), SHGFI_DISPLAYNAME))
        return QString::fromWCharArray(info.szDisplayName);
    return QFileInfo(path).fileName();
}
bool StartMenuModel::activate(int row)
{
    if (row < 0 || row >= m_entries.size())
        return false;
    if (m_entries[row].isDir) {
        const QString path = m_entries[row].path;
        setDirectory(path);
        return false;
    }
    launch(row);
    return true;
}

bool StartMenuModel::isRoot() const { return m_isRoot; }

QString StartMenuModel::rootPath() const
{
    QStringList dirs;
    const QString user = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!user.isEmpty()) {
        dirs << user;
    }
    const QString machine
        = QString::fromLocal8Bit(qgetenv("ProgramData")) + QLatin1String("\\Microsoft\\Windows\\Start Menu\\Programs");
    if (!machine.isEmpty()) {
        dirs << machine;
    }
    return dirs.join(QLatin1Char(';'));
}

void StartMenuModel::setDirectory(const QString& path)
{
    const QString normalized = path.startsWith(QLatin1String("merged:")) || path.startsWith(QLatin1String("cat:"))
        ? path
        : QDir::fromNativeSeparators(path);
    if (m_directory == normalized && !m_categoryDirs.isEmpty() == normalized.startsWith(QLatin1String("cat:"))) {
        return;
    }
    m_directory = normalized;
    m_searchText.clear();
    Q_EMIT searchTextChanged();
    m_isRoot = path.isEmpty() || path == QLatin1String("all");
    if (m_isRoot) {
        m_stack.clear();
        m_categoryDirs.clear();
    } else if (normalized.startsWith(QLatin1String("cat:"))) {
        /* custom category: resolve its directories from the config */
        const QString catName = normalized.mid(4);
        m_categoryDirs.clear();
        for (const QJsonValue& cat : m_categories) {
            if (cat.toObject().value(QLatin1String("name")).toString() == catName) {
                const QJsonArray dirs = cat.toObject().value(QLatin1String("dirs")).toArray();
                for (const QJsonValue& d : dirs) {
                    const QString dir = QDir::fromNativeSeparators(d.toString());
                    if (!dir.isEmpty()) {
                        m_categoryDirs.append(dir);
                    }
                }
                break;
            }
        }
        m_stack.append(normalized);
    } else {
        m_categoryDirs.clear();
        m_stack.append(normalized);
    }
    scan();
    Q_EMIT directoryChanged();
}

void StartMenuModel::goParent()
{
    if (m_stack.isEmpty()) {
        return;
    }
    m_stack.removeLast();
    if (m_stack.isEmpty()) {
        m_directory.clear();
        m_isRoot = true;
        m_categoryDirs.clear();
    } else {
        m_directory = m_stack.last();
        m_categoryDirs.clear();
        if (m_directory.startsWith(QLatin1String("cat:"))) {
            for (const auto& category : m_categories)
                if (category.toObject().value(QLatin1String("name")).toString() == m_directory.mid(4)) {
                    for (const auto& path : category.toObject().value(QLatin1String("dirs")).toArray())
                        m_categoryDirs.append(path.toString());
                }
        }
    }
    m_searchText.clear();
    Q_EMIT searchTextChanged();
    scan();
    Q_EMIT directoryChanged();
}

void StartMenuModel::scan()
{
    beginResetModel();
    m_entries.clear();

    // Ensure COM is usable on this thread for IShellLink (idempotent).
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (m_isRoot) {
        struct Tool {
            const char* role;
            const char* executable;
            const char* desktop;
        };
        for (const auto& tool : { Tool { "konsole", "konsole.exe", "org.kde.konsole.desktop" },
                 Tool { "process-monitor", "plasma-process-monitor.exe", "org.nixbridge.processmonitor.desktop" },
                 Tool { "dolphin", "dolphin.exe", "org.kde.dolphin.desktop" },
                 Tool { "text-editor", "plasma-text-editor.exe", "org.nixbridge.texteditor.desktop" },
                 Tool { "system-settings", "plasma-system-settings.exe", "org.nixbridge.systemsettings.desktop" } }) {
            const QString executable
                = QCoreApplication::applicationDirPath() + QLatin1Char('/') + QLatin1String(tool.executable);
            if (!QFileInfo::exists(executable))
                continue;
            KDesktopFile desktop(QCoreApplication::applicationDirPath() + QStringLiteral("/data/applications/")
                + QLatin1String(tool.desktop));
            Entry entry;
            entry.name = desktop.readName();
            entry.exec = executable;
            entry.linkPath = QStringLiteral("kde:") + QLatin1String(tool.role);
            entry.path = entry.linkPath;
            if (entry.name.isEmpty())
                entry.name = QLatin1String(tool.role);
            m_entries.append(entry);
        }
    }

    QStringList dirs;
    if (m_isRoot) {
        dirs << rootPath().split(QLatin1Char(';'), Qt::SkipEmptyParts);
    } else if (!m_categoryDirs.isEmpty()) {
        dirs = m_categoryDirs;
    } else if (m_directory.startsWith(QLatin1String("merged:"))) {
        const auto values = QJsonDocument::fromJson(m_directory.mid(7).toUtf8()).array();
        for (const auto& value : values)
            dirs.append(value.toString());
    } else {
        dirs << m_directory;
    }

    QMap<QString, QStringList> subdirs;
    QSet<QString> seenLinks;
    for (const QString& dir : dirs) {
        QDir d(dir);
        if (!d.exists()) {
            continue;
        }
        const QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                subdirs[fi.fileName().toCaseFolded()].append(fi.absoluteFilePath());
            } else if (!m_isRoot
                || QStringList { "lnk", "url", "appref-ms", "exe", "cmd", "bat", "msc", "cpl" }.contains(
                    fi.suffix().toLower())) {
                if (fi.fileName().compare(QLatin1String("desktop.ini"), Qt::CaseInsensitive) == 0
                    || fi.fileName().compare(QLatin1String("thumbs.db"), Qt::CaseInsensitive) == 0)
                    continue;
                Entry e;
                e.isDir = false;
                e.linkPath = fi.absoluteFilePath();
                QString target;
                if (fi.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0)
                    e.name = resolveLnk(fi.absoluteFilePath(), &target);
                else
                    e.name = QStringList { "exe", "cmd", "bat", "msc", "cpl" }.contains(fi.suffix().toLower())
                        ? fi.completeBaseName()
                        : fi.fileName();
                e.exec = target.isEmpty() ? fi.absoluteFilePath() : target;
                e.path = fi.absoluteFilePath();
                const QString identity = (e.name + QLatin1Char('|') + e.exec).toCaseFolded() + QLatin1Char('|')
                    + readWindowsShortcut(e.linkPath).arguments;
                if (seenLinks.contains(identity))
                    continue;
                seenLinks.insert(identity);
                m_entries.append(e);
            }
        }
    }
    for (const QStringList& paths : std::as_const(subdirs)) {
        const QString sd = paths.first();
        Entry e;
        e.isDir = true;
        e.name = QFileInfo(sd).fileName();
        SHFILEINFOW info {};
        if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(sd.utf16()), 0, &info, sizeof(info), SHGFI_DISPLAYNAME))
            e.name = QString::fromWCharArray(info.szDisplayName);
        e.path = paths.size() == 1 ? sd
                                   : QStringLiteral("merged:")
                + QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact));
        const QString name = QFileInfo(sd).fileName().toLower();
        e.icon = name.contains(QLatin1String("accessories")) ? QStringLiteral("applications-utilities")
            : name.contains(QLatin1String("administrative")) || name.contains(QLatin1String("system tools"))
                || name.contains(QLatin1String("windows tools"))
            ? QStringLiteral("preferences-system")
            : name.contains(QLatin1String("startup")) ? QStringLiteral("system-run")
                                                      : QStringLiteral("folder");
        m_entries.append(e);
    }

    /* UWP (Store) apps live in the virtual AppsFolder, not as .lnk
       files - append them once when showing the root view. */
    if (m_isRoot && !m_uwpLoaded) {
        const int start = m_entries.size();
        scanUwpApps();
        m_uwpEntries = m_entries.mid(start);
        m_uwpLoaded = true;
    } else if (m_isRoot) {
        m_entries.append(m_uwpEntries);
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(m_entries.begin(), m_entries.end(), [&collator](const Entry& a, const Entry& b) {
        if (a.isDir != b.isDir)
            return a.isDir;
        return collator.compare(a.name, b.name) < 0;
    });
    m_allEntries = m_entries;
    if (!m_searchText.isEmpty())
        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                            [this](const Entry& e) { return !e.name.contains(m_searchText, Qt::CaseInsensitive); }),
            m_entries.end());
    endResetModel();
}

void StartMenuModel::setSearchText(const QString& text)
{
    if (text == m_searchText)
        return;
    m_searchText = text;
    beginResetModel();
    m_entries = m_allEntries;
    if (!text.isEmpty())
        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                            [&text](const Entry& e) { return !e.name.contains(text, Qt::CaseInsensitive); }),
            m_entries.end());
    endResetModel();
    Q_EMIT searchTextChanged();
}

void StartMenuModel::reveal(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return;
    const auto& entry = m_entries[row];
    QString directory = entry.isDir ? entry.path : QFileInfo(entry.linkPath).absolutePath();
    if (entry.linkPath.startsWith(QLatin1String("kde:")))
        directory = QCoreApplication::applicationDirPath();
    if (directory.startsWith(QLatin1String("merged:")))
        directory = QJsonDocument::fromJson(directory.mid(7).toUtf8()).array().toVariantList().value(0).toString();
    if (directory.isEmpty())
        return;
    if (entry.linkPath.startsWith(QLatin1String("apps:"))) {
        ShellExecuteW(nullptr, L"open", L"shell:AppsFolder", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    QProcess::startDetached(QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-kde-launcher.exe"),
        { QStringLiteral("dolphin"), directory });
}

void StartMenuModel::scanUwpApps()
{
    IShellFolder* desktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&desktop))) {
        return;
    }
    PIDLIST_ABSOLUTE appsPidl = nullptr;
    if (FAILED(SHParseDisplayName(L"shell:AppsFolder", nullptr, &appsPidl, 0, nullptr))) {
        desktop->Release();
        return;
    }
    IShellFolder* appsFolder = nullptr;
    if (FAILED(desktop->BindToObject(appsPidl, nullptr, IID_IShellFolder, reinterpret_cast<void**>(&appsFolder)))) {
        CoTaskMemFree(appsPidl);
        desktop->Release();
        return;
    }
    IEnumIDList* en = nullptr;
    if (SUCCEEDED(appsFolder->EnumObjects(nullptr, SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &en))) {
        PITEMID_CHILD child = nullptr;
        while (en->Next(1, &child, nullptr) == S_OK) {
            PIDLIST_ABSOLUTE full = ILCombine(appsPidl, child);
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromIDList(full, IID_IShellItem, reinterpret_cast<void**>(&item)))) {
                PWSTR displayName = nullptr;
                PWSTR parsingName = nullptr;
                item->GetDisplayName(SIGDN_NORMALDISPLAY, &displayName);
                item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsingName);
                const QString display = displayName ? QString::fromWCharArray(displayName) : QString();
                const QString parsing = parsingName ? QString::fromWCharArray(parsingName) : QString();
                CoTaskMemFree(displayName);
                CoTaskMemFree(parsingName);
                item->Release();
                if (!display.isEmpty() && !parsing.isEmpty()
                    && !parsing.contains(QLatin1String(".exe"), Qt::CaseInsensitive)
                    && !parsing.startsWith(QLatin1String("C:"), Qt::CaseInsensitive)
                    && !parsing.startsWith(QLatin1String("shell:"), Qt::CaseInsensitive)) {
                    /* AUMID -> shell:AppsFolder launch; the icon provider
                       handles the "apps:" linkPath prefix */
                    Entry e;
                    e.isDir = false;
                    e.name = display;
                    e.exec = QStringLiteral("explorer.exe \"shell:AppsFolder\\%1\"").arg(parsing);
                    e.path = e.linkPath = QStringLiteral("apps:%1").arg(parsing);
                    m_entries.append(e);
                }
            }
            CoTaskMemFree(child);
            CoTaskMemFree(full);
        }
        en->Release();
    }
    appsFolder->Release();
    CoTaskMemFree(appsPidl);
    desktop->Release();
}

void StartMenuModel::loadCategories()
{
    m_categories = QJsonArray();
    const QString configPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QLatin1String("/plasma/startmenu-categories.json");
    QFile f(configPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    m_categories = doc.object().value(QLatin1String("sidebar")).toArray();
}

QVariantList StartMenuModel::categories() const
{
    QVariantList out;
    for (const QJsonValue& cat : m_categories) {
        const QJsonObject obj = cat.toObject();
        QVariantMap m;
        m.insert(QLatin1String("name"), obj.value(QLatin1String("name")).toString());
        m.insert(QLatin1String("icon"), obj.value(QLatin1String("icon")).toString());
        out.append(m);
    }
    return out;
}

void StartMenuModel::reload()
{
    loadCategories();
    m_uwpLoaded = false;
    scan();
    Q_EMIT directoryChanged();
}

QString StartMenuModel::resolveLnk(const QString& lnkPath, QString* targetOut) const
{
    const auto shortcut = readWindowsShortcut(lnkPath);
    if (targetOut)
        *targetOut = shortcut.target;
    // Shortcut descriptions are tooltips, not application names.
    SHFILEINFOW info {};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(lnkPath.utf16()), 0, &info, sizeof(info), SHGFI_DISPLAYNAME)) {
        QString name = QString::fromWCharArray(info.szDisplayName);
        if (name.endsWith(QLatin1String(".lnk"), Qt::CaseInsensitive))
            name.chop(4);
        if (!name.isEmpty())
            return name;
    }
    return QFileInfo(lnkPath).completeBaseName();
}

void StartMenuModel::launch(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    const Entry& e = m_entries.at(row);
    if (e.isDir || e.exec.isEmpty()) {
        return;
    }
    if (e.linkPath.startsWith(QLatin1String("kde:"))) {
        QProcess::startDetached(
            QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-kde-launcher.exe"), { e.linkPath.mid(4) });
        return;
    }
    if (e.linkPath.startsWith(QLatin1String("apps:"))) {
        const QString id = e.linkPath.toLower();
        const QString role = id.contains(QLatin1String("microsoft.windowsnotepad_")) ? QStringLiteral("text-editor")
            : id.contains(QLatin1String("windows.immersivecontrolpanel_"))           ? QStringLiteral("system-settings")
                                                                                     : QString();
        if (!role.isEmpty()) {
            QProcess::startDetached(
                QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-kde-launcher.exe"), { role });
            return;
        }
    }
    // Standard Windows shell/task-manager shortcuts use the matching KDE tool.
    // Keep customized shortcut command lines on their original launch path.
    if (!e.linkPath.startsWith(QLatin1String("apps:"))
        && readWindowsShortcut(e.linkPath).arguments.trimmed().isEmpty()) {
        const QString program = QFileInfo(QDir::fromNativeSeparators(e.exec)).fileName().toLower();
        const QString bin = QCoreApplication::applicationDirPath();
        if (program == QLatin1String("notepad.exe") || program == QLatin1String("control.exe")
            || program == QLatin1String("systemsettings.exe")) {
            const QString role = program == QLatin1String("notepad.exe") ? QStringLiteral("text-editor")
                                                                         : QStringLiteral("system-settings");
            QProcess::startDetached(bin + QStringLiteral("/plasma-kde-launcher.exe"), { role });
            return;
        }
        if (program == QLatin1String("taskmgr.exe")
            && QFileInfo::exists(bin + QStringLiteral("/plasma-process-monitor.exe"))) {
            QProcess::startDetached(
                bin + QStringLiteral("/plasma-kde-launcher.exe"), { QStringLiteral("process-monitor") });
            return;
        }
        if (QStringList { QStringLiteral("cmd.exe"), QStringLiteral("powershell.exe"), QStringLiteral("pwsh.exe") }
                .contains(program)
            && QFileInfo::exists(bin + QStringLiteral("/konsole.exe"))) {
            const QString shell = QFileInfo::exists(e.exec) ? e.exec : QStandardPaths::findExecutable(program);
            if (!shell.isEmpty()) {
                QProcess::startDetached(bin + QStringLiteral("/plasma-kde-launcher.exe"),
                    { QStringLiteral("konsole"), QStringLiteral("-e"), QDir::toNativeSeparators(shell) });
                return;
            }
        }
    }
    if (e.linkPath.startsWith(QLatin1String("apps:"))) {
        /* UWP: activate via IApplicationActivationManager - the
           explorer.exe shell:AppsFolder trick fails with
           "class not registered" for several apps */
        launchUwp(e.linkPath.mid(5));
        return;
    }
    const QString shortcut = QDir::toNativeSeparators(e.linkPath);
    SHELLEXECUTEINFOW info {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = reinterpret_cast<LPCWSTR>(shortcut.utf16());
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        qWarning() << "Windows Start Menu launch failed" << shortcut << GetLastError();
    }
}

void StartMenuModel::launchUwp(const QString& aumid) const
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IApplicationActivationManager* manager = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER,
            IID_IApplicationActivationManager, reinterpret_cast<void**>(&manager)))) {
        DWORD pid = 0;
        manager->ActivateApplication(reinterpret_cast<LPCWSTR>(aumid.utf16()), nullptr, AO_NONE, &pid);
        manager->Release();
    }
}
