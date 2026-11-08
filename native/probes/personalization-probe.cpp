// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJSEngine>
#include <QTextStream>
#include "../apps/personalization.h"
int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    QSettings settings(directory.filePath(QStringLiteral("prefs.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("pendingPanel"), true);
    settings.setValue(QStringLiteral("panelHeight"), 999);
    settings.setValue(QStringLiteral("panelLocation"), QStringLiteral("';throw 'injection"));
    QJSEngine engine;
    engine.evaluate(QStringLiteral("var p={height:77};var d={writeConfig:function(k,v){this.image=v}};function "
                                   "panels(){return[p]}function desktops(){return[d]}function print(s){result=s}"));
    const auto result = engine.evaluate(personalizationScript(settings));
    bool ok = !result.isError() && engine.globalObject().property("p").property("height").toInt() == 96
        && engine.globalObject().property("p").property("location").toString() == QStringLiteral("bottom");
    settings.setValue(QStringLiteral("pendingPanel"), false);
    settings.setValue(QStringLiteral("pendingWallpaper"), true);
    const QString wallpaper = directory.filePath(QStringLiteral("quote' 中文 image.png"));
    settings.setValue(QStringLiteral("wallpaper"), wallpaper);
    engine.evaluate(QStringLiteral("p.height=77"));
    ok &= !engine.evaluate(personalizationScript(settings)).isError();
    ok &= engine.globalObject().property("p").property("height").toInt() == 77;
    ok &= engine.globalObject().property("d").property("image").toString() == QUrl::fromLocalFile(wallpaper).toString();
    ok &= engine.globalObject().property("result").toString() == QStringLiteral("PERSONALIZATION_APPLIED");
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": real personalization scripts execute; bounds, option validation, quoted Unicode paths, "
                           "wallpaper-only preserves panels\n";
    return ok ? 0 : 1;
}
