// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QElapsedTimer>
#include <QThread>
#include <QFontDatabase>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTextStream>
#include <KIconLoader>
#include <memory>
#include <Plasma/Theme>
#include "../apps/appearance-config.h"
int main(int argc, char** argv)
{
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;
    const QString prefsFile = directory.filePath("appearance.ini");
    qputenv("KDE_ON_WIN_SETTINGS", prefsFile.toUtf8());
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    QSettings preferences(prefsFile, QSettings::IniFormat);
    preferences.setValue("dark", false);
    preferences.setValue("fontSize", 10);
    preferences.setValue("language", "zh_CN");
    preferences.sync();
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
    const QString base = app.applicationDirPath() + "/data/color-schemes/";
    const QString globals = directory.filePath("kdeglobals"), plasma = directory.filePath("plasmarc"),
                  local = directory.filePath("current.colors");
    {
        KConfig config(globals, KConfig::SimpleConfig);
        config.group("Unrelated").writeEntry("Keep", QStringLiteral("user preference"));
        config.sync();
    }
    bool ok = writeAppearanceConfig(base + "BreezeLight.colors", globals, plasma, QFont("Segoe UI", 10), {}, local);
    auto pump = [&] {
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < 1600) {
            app.processEvents();
            QThread::msleep(5);
        }
    };
    pump();
    Plasma::Theme desktop(QStringLiteral("default"));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport org.kde.kirigami as Kirigami\nItem {property color background: "
                      "Kirigami.Theme.backgroundColor; property color highlight: Kirigami.Theme.highlightColor; "
                      "property font uiFont: Kirigami.Theme.defaultFont}",
        QUrl());
    QQuickWindow hidden;
    std::unique_ptr<QObject> item(component.create());
    if (!item) {
        QTextStream(stderr) << component.errorString();
        return 1;
    }
    if (auto* quick = qobject_cast<QQuickItem*>(item.get()))
        quick->setParentItem(hidden.contentItem());
    pump();
    auto iconBrightness = [] {
        auto image = KIconLoader::global()->loadIcon("window-close", KIconLoader::Toolbar, 16).toImage();
        double sum = 0, weight = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto color = image.pixelColor(x, y);
                sum += color.lightnessF() * color.alphaF();
                weight += color.alphaF();
            }
        return weight ? sum / weight : -1.0;
    };
    const double lightIcon = iconBrightness();
    const QColor light = app.palette().color(QPalette::Window);
    ok &= light.lightnessF() > 0.6 && desktop.color(Plasma::Theme::BackgroundColor).lightnessF() > 0.6
        && item->property("background").value<QColor>().lightnessF() > 0.6;
    const QColor accent("#c751a3");
    preferences.setValue("dark", true);
    preferences.setValue("fontSize", 14);
    preferences.setValue("fontFamily", "Segoe UI");
    preferences.setValue("accentColor", accent.name());
    preferences.sync();
    ok &= writeAppearanceConfig(base + "BreezeDark.colors", globals, plasma, QFont("Segoe UI", 14), accent, local);
    pump();
    const double darkIcon = iconBrightness();
    ok &= lightIcon >= 0 && lightIcon < 0.5 && darkIcon > 0.6;
    QTextStream(stdout) << "caption icon brightness light=" << lightIcon << " dark=" << darkIcon << '\n';
    const auto widget = app.palette().color(QPalette::Window), panel = desktop.color(Plasma::Theme::BackgroundColor),
               menu = item->property("background").value<QColor>();
    QTextStream(stdout) << "dark widget=" << widget.name() << " plasma=" << panel.name() << " kirigami=" << menu.name()
                        << " accent=" << desktop.color(Plasma::Theme::HighlightColor).name()
                        << " font=" << app.font().pointSize()
                        << " qmlFont=" << item->property("uiFont").value<QFont>().pointSize() << '\n';
    ok &= widget.lightnessF() < 0.3 && panel.lightnessF() < 0.3 && menu.lightnessF() < 0.3
        && desktop.color(Plasma::Theme::FocusColor) == accent && app.palette().color(QPalette::Accent) == accent
        && desktop.color(Plasma::Theme::HighlightColor) == accent
        && item->property("highlight").value<QColor>() == accent && app.font().pointSize() == 14
        && item->property("uiFont").value<QFont>().pointSize() == 14;
    {
        KConfig config(globals, KConfig::SimpleConfig);
        ok &= config.group("Unrelated").readEntry("Keep", QString()) == "user preference";
    }
    preferences.setValue("dark", false);
    preferences.remove("accentColor");
    preferences.sync();
    ok &= writeAppearanceConfig(base + "BreezeLight.colors", globals, plasma, QFont("Segoe UI", 10), {}, local);
    pump();
    ok &= app.palette().color(QPalette::Window).lightnessF() > 0.6
        && desktop.color(Plasma::Theme::BackgroundColor).lightnessF() > 0.6
        && item->property("background").value<QColor>().lightnessF() > 0.6;
    QTextStream(stdout)
        << (ok ? "PASS" : "FAIL")
        << ": live QWidget, Plasma theme and Kirigami light/dark/accent/font synchronization; temporary files only\n";
    return ok ? 0 : 1;
}
