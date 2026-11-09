// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
inline QString scriptString(const QString& text)
{
    const QByteArray json = QJsonDocument(QJsonArray { text }).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}
inline QString personalizationScript(QSettings& settings)
{
    const int height = qBound(24, settings.value(QStringLiteral("panelHeight"), 44).toInt(), 96);
    QString location = settings.value(QStringLiteral("panelLocation"), QStringLiteral("bottom")).toString();
    if (!QStringList { "top", "bottom", "left", "right" }.contains(location))
        location = QStringLiteral("bottom");
    QString hiding = settings.value(QStringLiteral("panelHiding"), QStringLiteral("none")).toString();
    if (!QStringList { "none", "autohide", "dodgewindows" }.contains(hiding))
        hiding = QStringLiteral("none");
    QString opacity = settings.value(QStringLiteral("panelOpacity"), QStringLiteral("opaque")).toString();
    if (!QStringList { "opaque", "translucent", "adaptive" }.contains(opacity))
        opacity = QStringLiteral("opaque");
    const bool panel = settings.value(QStringLiteral("pendingPanel"), false).toBool();
    const bool desktop = settings.value(QStringLiteral("pendingWallpaper"), false).toBool();
    QString script = QStringLiteral(
        "var ps=panels();var ds=desktops();if((%1&&!ps.length)||(%2&&!ds.length)){print('WAITING_PLASMA');}else{")
                         .arg(panel ? QStringLiteral("true") : QStringLiteral("false"),
                             desktop ? QStringLiteral("true") : QStringLiteral("false"));
    if (settings.value("pendingAppearance", false).toBool())
        script += QStringLiteral("theme=\"default\"; ");
    if (panel)
        script += QStringLiteral("for(var "
                                 "i=0;i<ps.length;i++){ps[i].height=%1;ps[i].location=%2;ps[i].hiding=%3;ps[i].opacity="
                                 "%4;ps[i].floating=%5;} ")
                      .arg(height)
                      .arg(scriptString(location), scriptString(hiding), scriptString(opacity),
                          settings.value(QStringLiteral("panelFloating"), false).toBool() ? QStringLiteral("true")
                                                                                          : QStringLiteral("false"));
    const QString wallpaper = settings.value(QStringLiteral("wallpaper")).toString();
    if (desktop && !wallpaper.isEmpty())
        script += QStringLiteral("for(var "
                                 "j=0;j<ds.length;j++){ds[j].wallpaperPlugin='org.kde.image';ds[j].currentConfigGroup=["
                                 "'Wallpaper','org.kde.image','General'];ds[j].writeConfig('Image',%1);}")
                      .arg(scriptString(QUrl::fromLocalFile(wallpaper).toString()));
    return script + QStringLiteral("print('PERSONALIZATION_APPLIED');}");
}
