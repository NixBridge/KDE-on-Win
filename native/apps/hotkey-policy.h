// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QSet>
#include <windows.h>
inline bool uniqueHotkeys(const QString& keys)
{
    QSet<QChar> seen;
    for (const auto key : keys) {
        if (seen.contains(key))
            return false;
        seen.insert(key);
    }
    return true;
}
inline QString hotkeyFailure(DWORD error)
{
    if (error == ERROR_HOTKEY_ALREADY_REGISTERED)
        return QObject::tr("Already registered by another shortcut (1409)");
    if (error == ERROR_ACCESS_DENIED)
        return QObject::tr("Access denied (5)");
    return QObject::tr("Windows error %1").arg(error);
}
