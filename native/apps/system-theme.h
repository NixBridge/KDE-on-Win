// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QUuid>
#include <optional>
#include <windows.h>
inline std::optional<bool> windowsDarkPreference()
{
    DWORD light = 1, size = sizeof(light);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size)
        != ERROR_SUCCESS)
        return {};
    HIGHCONTRASTW contrast { sizeof(contrast) };
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON))
        return {};
    return light == 0;
}
inline bool currentThemeIsBreeze()
{
    const auto globals = KSharedConfig::openConfig("kdeglobals");
    globals->reparseConfiguration();
    const auto scheme = globals->group("General").readEntry("ColorScheme", QString());
    const auto plasma = KSharedConfig::openConfig("plasmarc");
    plasma->reparseConfiguration();
    const auto theme = plasma->group("Theme").readEntry("name", QString());
    return (scheme.isEmpty() || QStringList { "Breeze", "BreezeLight", "BreezeDark" }.contains(scheme))
        && (theme.isEmpty() || QStringList { "default", "breeze", "breeze-dark", "breeze-light" }.contains(theme));
}
// Shared by the resident tool and pure tests. Never writes when following is disabled or a custom theme is active.
inline bool syncWindowsTheme(QSettings& settings, std::optional<bool> dark, bool supported)
{
    settings.sync();
    if (settings.status() != QSettings::NoError || !settings.value("followWindowsTheme", false).toBool() || !dark
        || !supported || settings.value("dark", false).toBool() == *dark)
        return false;
    settings.setValue("dark", *dark);
    settings.setValue("pendingAppearance", true);
    settings.setValue("pendingPersonalization", true);
    settings.setValue("personalizationRevision", QUuid::createUuid().toString(QUuid::WithoutBraces));
    settings.remove("personalizationError");
    settings.sync();
    return settings.status() == QSettings::NoError;
}
