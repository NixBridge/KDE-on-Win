// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <shlobj.h>

struct FolderRoute {
    QString directory;
    QString shellNamespace;
};

inline FolderRoute resolveWindowsFolder(QString input)
{
    if (input.endsWith(QLatin1String("\\.")))
        input.chop(2); // Registry quoting suffix, including shell namespace names.
    if (input.isEmpty())
        input = QDir::homePath();
    if (input.compare(QLatin1String("desktop:/"), Qt::CaseInsensitive) == 0
        || input.compare(QLatin1String("shell:Desktop"), Qt::CaseInsensitive) == 0
        || input.compare(QLatin1String("::{00021400-0000-0000-C000-000000000046}"), Qt::CaseInsensitive) == 0)
        input = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (input.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        input = QUrl(input).toLocalFile();
    // Qt treats directory shortcuts as isDir(), while returning the .lnk
    // filename. Native attributes distinguish actual directories from shortcuts.
    const auto native = QDir::toNativeSeparators(input);
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return { QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(input).absoluteFilePath())), {} };

    // Follow directory shortcuts without opening the shortcut or running code.
    if (QFileInfo(input).suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0) {
        QString directory;
        IShellLinkW* link = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
            IPersistFile* persist = nullptr;
            if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&persist)))) {
                if (SUCCEEDED(persist->Load(reinterpret_cast<LPCWSTR>(native.utf16()), STGM_READ))) {
                    PIDLIST_ABSOLUTE id = nullptr;
                    if (SUCCEEDED(link->GetIDList(&id)) && id) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(SHCreateItemFromIDList(id, IID_PPV_ARGS(&item)))) {
                            PWSTR path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                                directory = QString::fromWCharArray(path);
                                CoTaskMemFree(path);
                            }
                            item->Release();
                        }
                        CoTaskMemFree(id);
                    }
                }
                persist->Release();
            }
            link->Release();
        }
        if (!directory.isEmpty() && QFileInfo(directory).isDir())
            return { QDir::cleanPath(QDir::fromNativeSeparators(directory)), {} };
        return {};
    }
    if (!input.startsWith(QLatin1String("shell:"), Qt::CaseInsensitive) && !input.startsWith(QLatin1String("::{")))
        return {};
    IShellItem* item = nullptr;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(native.utf16()), nullptr, IID_PPV_ARGS(&item))))
        return {};
    PWSTR path = nullptr;
    FolderRoute result;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        const QString directory = QString::fromWCharArray(path);
        CoTaskMemFree(path);
        if (QFileInfo(directory).isDir())
            result.directory = QDir::cleanPath(QDir::fromNativeSeparators(directory));
    } else {
        SFGAOF attributes = 0;
        if (SUCCEEDED(item->GetAttributes(SFGAO_FOLDER, &attributes)) && (attributes & SFGAO_FOLDER))
            result.shellNamespace = input; // Windows-only virtual UI, not a filesystem folder.
    }
    item->Release();
    return result;
}
