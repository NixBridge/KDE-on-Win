// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
inline QString appearanceSettingsPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-win-session");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/appearance.ini");
}
