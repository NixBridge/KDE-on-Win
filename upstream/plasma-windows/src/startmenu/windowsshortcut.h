// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
#pragma once
#include <QString>
struct WindowsShortcut {
    QString target;
    QString description;
    QString arguments;
    QString iconLocation;
    int iconIndex = 0;
};
WindowsShortcut readWindowsShortcut(const QString &path);
