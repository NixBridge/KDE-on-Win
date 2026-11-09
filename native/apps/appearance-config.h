// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <KConfig>
#include <KConfigGroup>
#include <QFont>
#include <QColor>
#include <QFileInfo>
// Paths are arguments so tests use only temporary configuration files.
inline bool writeAppearanceConfig(const QString& schemeFile, const QString& globalsFile, const QString& plasmaFile,
    const QFont& font, const QColor& accent, const QString& localSchemeFile = {})
{
    if (!QFileInfo::exists(schemeFile))
        return false;
    KConfig source(schemeFile, KConfig::SimpleConfig), globals(globalsFile, KConfig::SimpleConfig),
        plasma(plasmaFile, KConfig::SimpleConfig);
    for (const QString& name : source.groupList()) {
        if (!name.startsWith("Colors:") && !name.startsWith("ColorEffects:") && name != "WM")
            continue;
        KConfigGroup target(&globals, name);
        const auto entries = source.group(name).entryMap();
        for (auto it = entries.cbegin(); it != entries.cend(); ++it)
            target.writeEntry(it.key(), it.value(), KConfig::Notify);
    }
    KConfigGroup general(&globals, "General");
    general.writeEntry("ColorScheme", QFileInfo(schemeFile).completeBaseName(), KConfig::Notify);
    general.writeEntry("font", font.toString(), KConfig::Notify);
    if (accent.isValid()) {
        const QString rgb = QStringLiteral("%1,%2,%3").arg(accent.red()).arg(accent.green()).arg(accent.blue());
        general.writeEntry("AccentColor", rgb, KConfig::Notify);
        for (const QString& name : { QStringLiteral("Colors:Button"), QStringLiteral("Colors:View"),
                 QStringLiteral("Colors:Window"), QStringLiteral("Colors:Header"), QStringLiteral("Colors:Tooltip") }) {
            auto group = globals.group(name);
            group.writeEntry("DecorationFocus", rgb, KConfig::Notify);
            group.writeEntry("DecorationHover", rgb, KConfig::Notify);
            group.writeEntry("ForegroundLink", rgb, KConfig::Notify);
            group.writeEntry("ForegroundActive", rgb, KConfig::Notify);
        }
    } else
        general.deleteEntry("AccentColor", KConfig::Notify);
    if (accent.isValid()) {
        KConfigGroup selection(&globals, "Colors:Selection");
        selection.writeEntry("BackgroundNormal",
            QStringLiteral("%1,%2,%3").arg(accent.red()).arg(accent.green()).arg(accent.blue()), KConfig::Notify);
        const QColor foreground = accent.lightnessF() > 0.6 ? QColor(Qt::black) : QColor(Qt::white);
        selection.writeEntry("ForegroundNormal",
            QStringLiteral("%1,%2,%3").arg(foreground.red()).arg(foreground.green()).arg(foreground.blue()),
            KConfig::Notify);
    }
    // Breeze's default desktop theme follows the application color palette.
    plasma.group("Theme").writeEntry("name", QStringLiteral("default"), KConfig::Notify);
    if (!globals.sync() || !plasma.sync())
        return false;
    if (!localSchemeFile.isEmpty()) {
        KConfig local(localSchemeFile, KConfig::SimpleConfig);
        for (const auto& name : globals.groupList()) {
            if (!name.startsWith("Colors:") && !name.startsWith("ColorEffects:") && name != "WM")
                continue;
            auto target = local.group(name);
            const auto entries = globals.group(name).entryMap();
            for (auto it = entries.cbegin(); it != entries.cend(); ++it)
                target.writeEntry(it.key(), it.value(), KConfig::Notify);
        }
        local.group("General").writeEntry("ColorScheme", QFileInfo(schemeFile).completeBaseName(), KConfig::Notify);
        if (!local.sync())
            return false;
    }
    return true;
}
