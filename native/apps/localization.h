// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
inline QString projectLanguage()
{
    QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
    const QString preference = settings.value("language", "system").toString();
    const QStringList languages = QLocale::system().uiLanguages();
    return preference == "system" ? (languages.isEmpty() ? QLocale::system().name() : languages.first()) : preference;
}
inline QString defaultProjectFontFamily()
{
    return projectLanguage().startsWith("zh") ? QStringLiteral("Microsoft YaHei UI") : QStringLiteral("Segoe UI");
}
inline void installProjectTranslations(QCoreApplication* app)
{
    if (app->property("projectTranslationsInstalled").toBool())
        return;
    const QString language = projectLanguage();
    if (language.startsWith("zh")) {
        for (const QString& file : { QStringLiteral("/../translations/qtbase_zh_CN.qm"),
                 QStringLiteral("/data/kde-on-win/translations/kdeonwin_zh_CN.qm") }) {
            auto* translator = new QTranslator(app);
            if (translator->load(app->applicationDirPath() + file))
                app->installTranslator(translator);
            else
                delete translator;
        }
    }
    app->setProperty("projectTranslationsInstalled", true);
}
