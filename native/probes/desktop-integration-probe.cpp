// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>
#include "../apps/desktop-actions.h"
#include "../apps/system-theme.h"
#include "../apps/hotkey-policy.h"
#include "../apps/diagnostics.h"
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return 2;
    const auto file = temp.filePath("preferences.ini");
    QSettings prefs(file, QSettings::IniFormat);
    prefs.setValue("dark", false);
    prefs.setValue("accentColor", "#123456");
    prefs.setValue("wallpaper", "SECRET-PERSONAL-PATH");
    bool ok = !syncWindowsTheme(prefs, true, true);
    prefs.setValue("followWindowsTheme", true);
    ok &= !syncWindowsTheme(prefs, {}, true) && !syncWindowsTheme(prefs, true, false);
    ok &= syncWindowsTheme(prefs, true, true) && prefs.value("pendingAppearance").toBool();
    const auto revision = prefs.value("personalizationRevision").toString();
    ok &= !syncWindowsTheme(prefs, true, true) && prefs.value("personalizationRevision").toString() == revision
        && prefs.value("accentColor").toString() == "#123456";
    prefs.setValue("followWindowsTheme", false);
    ok &= !syncWindowsTheme(prefs, false, true) && prefs.value("dark").toBool();
    ok &= uniqueHotkeys("ERDV") && !uniqueHotkeys("EERV") && hotkeyFailure(1409).contains("1409")
        && hotkeyFailure(5).contains("5");
    QSet<QString> ids;
    for (const auto& group : { desktopPlaces(), desktopQuickActions() })
        for (const auto& action : group) {
            ok &= !ids.contains(action.id);
            ids.insert(action.id);
            const auto plan = desktopActionPlan(action.id, temp.path());
            ok &= plan.error.isEmpty() && !plan.program.isEmpty();
            if (QStringList { "home", "downloads", "documents", "pictures", "desktop" }.contains(action.id))
                ok &= plan.program.endsWith("dolphin.exe") && QFileInfo(plan.arguments.value(0)).isAbsolute();
        }
    ok &= !desktopActionPlan("invalid").error.isEmpty() && !desktopActionPlan("computer").shell
        && desktopActionPlan("computer").arguments.first() == "shell:MyComputerFolder";
    const auto logs = temp.filePath("logs");
    ok &= QDir().mkpath(logs);
    QFile log(logs + "/session-shell.log");
    ok &= log.open(QIODevice::WriteOnly);
    log.write("error SECRET-TOKEN user C:/private/user password=SECRET-TOKEN\n");
    log.close();
    const auto report = diagnosticReport(temp.path(), file, logs);
    const auto json = QJsonDocument(report).toJson();
    ok &= !json.contains("SECRET") && !json.contains(temp.path().toUtf8()) && !json.contains("C:/private")
        && report.value("logs").toArray().size() == 1 && report.value("components").toArray().size() == 7;
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": Known Folder action plans, theme opt-in/custom-theme protection/idempotence, shortcut "
                           "errors and bounded private diagnostic summary; no actions executed\n";
    return ok ? 0 : 1;
}
