// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
#include "windowsshortcut.h"
#include <QDir>
#include <shlobj.h>

WindowsShortcut readWindowsShortcut(const QString &path)
{
    WindowsShortcut result;
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return result;
    IPersistFile *persist = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&persist)))) {
        const QString native = QDir::toNativeSeparators(path);
        if (SUCCEEDED(persist->Load(reinterpret_cast<LPCWSTR>(native.utf16()), STGM_READ))) {
            wchar_t value[32768]{};
            if (SUCCEEDED(link->GetPath(value, 32768, nullptr, SLGP_RAWPATH))) result.target = QString::fromWCharArray(value);
            if (SUCCEEDED(link->GetDescription(value, 32768))) result.description = QString::fromWCharArray(value);
            if (SUCCEEDED(link->GetArguments(value, 32768))) result.arguments = QString::fromWCharArray(value);
            if (SUCCEEDED(link->GetIconLocation(value, 32768, &result.iconIndex))) result.iconLocation = QString::fromWCharArray(value);
        }
        persist->Release();
    }
    link->Release();
    return result;
}
