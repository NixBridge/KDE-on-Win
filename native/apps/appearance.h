// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QFileInfo>
inline QString appearanceSettingsPath()
{
    const QString overridePath = qEnvironmentVariable("KDE_ON_WIN_SETTINGS");
    if (QFileInfo(overridePath).isAbsolute())
        return overridePath;
    const QString directory
        = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-win-session");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/appearance.ini");
}
