// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QTextStream>
#include <QFontDatabase>
#include <KIconTheme>
#include <KIconLoader>
#include "appearance.h"

int main(int argc, char **argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows"); KIconTheme::initTheme(); QApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) {
        const int id = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+QStringLiteral("/Fonts/segoeui.ttf"));
        if (id >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(id).value(0), 10));
    }
    app.setDesktopFileName(QStringLiteral("org.nixbridge.systemsettings"));
    app.setWindowIcon(QIcon(KIconLoader::global()->loadIcon(QStringLiteral("preferences-system"), KIconLoader::Desktop, 48)));
    QMainWindow window; window.setWindowTitle(QObject::tr("System Settings")); window.resize(880, 600);
    auto *central = new QWidget(&window); auto *layout = new QHBoxLayout(central); window.setCentralWidget(central);
    auto *categories = new QListWidget(central); categories->setMaximumWidth(220); layout->addWidget(categories);
    auto *pages = new QStackedWidget(central); layout->addWidget(pages, 1);
    auto page = [&](const QString &name, const QString &icon) {
        categories->addItem(new QListWidgetItem(QIcon(KIconLoader::global()->loadIcon(icon, KIconLoader::Desktop, 24)), name));
        auto *widget = new QWidget(pages); auto *contents = new QVBoxLayout(widget);
        auto *heading = new QLabel(name, widget); QFont font = heading->font(); font.setPointSize(font.pointSize()+6); heading->setFont(font); contents->addWidget(heading); pages->addWidget(widget); return contents;
    };
    auto *appearance = page(QObject::tr("Appearance"), QStringLiteral("preferences-desktop-theme"));
    auto *description = new QLabel(QObject::tr("Choose the appearance of KDE-on-Win applications and standard window titlebars.")); description->setWordWrap(true); appearance->addWidget(description);
    QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
    auto *theme = new QComboBox; theme->addItem(QObject::tr("Breeze Light"), false); theme->addItem(QObject::tr("Breeze Dark"), true); theme->setCurrentIndex(settings.value(QStringLiteral("dark"), false).toBool() ? 1 : 0); appearance->addWidget(theme);
    auto *decoration = new QCheckBox(QObject::tr("Use Breeze titlebars on standard Windows windows")); decoration->setChecked(settings.value(QStringLiteral("decorateWindows"), true).toBool()); appearance->addWidget(decoration);
    auto *scope = new QLabel(QObject::tr("Custom titlebars are skipped. Application content keeps the style supported by its own toolkit.")); scope->setWordWrap(true); appearance->addWidget(scope); appearance->addStretch();
    auto save = [&] { settings.setValue(QStringLiteral("dark"), theme->currentData()); settings.setValue(QStringLiteral("decorateWindows"), decoration->isChecked()); settings.sync(); };
    QObject::connect(theme, &QComboBox::currentIndexChanged, &window, save); QObject::connect(decoration, &QCheckBox::toggled, &window, save);
    struct Category { const char *name; const char *icon; const char *uri; const char *description; };
    const Category links[] = {
        {"Display", "preferences-desktop-display", "ms-settings:display", "Resolution, scaling, orientation and monitors"},
        {"Sound", "preferences-desktop-sound", "ms-settings:sound", "Windows audio devices and volume"},
        {"Network", "preferences-system-network", "ms-settings:network-status", "Windows network connections"},
        {"Bluetooth", "preferences-system-bluetooth", "ms-settings:bluetooth", "Pair and manage Windows devices"},
        {"Language & Input", "preferences-desktop-locale", "ms-settings:regionlanguage", "Languages, keyboards and input methods"},
        {"Notifications", "preferences-desktop-notification", "ms-settings:notifications", "Windows notification and banner preferences"},
        {"Accounts", "preferences-system-users", "ms-settings:yourinfo", "Windows user account settings"},
        {"Power", "preferences-system-power-management", "ms-settings:powersleep", "Power, sleep and battery settings"},
        {"Updates", "system-software-update", "ms-settings:windowsupdate", "Windows Update settings"},
    };
    for (const auto &entry : links) {
        auto *contents = page(QObject::tr(entry.name), QLatin1String(entry.icon));
        auto *label = new QLabel(QObject::tr(entry.description)); label->setWordWrap(true); contents->addWidget(label);
        auto *button = new QPushButton(QObject::tr("Open Windows Settings")); contents->addWidget(button);
        const QUrl uri(QLatin1String(entry.uri)); QObject::connect(button, &QPushButton::clicked, &window, [uri] { QDesktopServices::openUrl(uri); }); contents->addStretch();
    }
    QObject::connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex); categories->setCurrentRow(0);
    window.show();
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) QTimer::singleShot(500, &window, [&] {
        bool ok = pages->count() == 10 && categories->count() == 10;
        categories->setCurrentRow(3); ok &= pages->currentIndex() == 3;
        ok &= window.findChild<QWidget*>(QStringLiteral("PlasmaBreezeTitleBar")) != nullptr;
        categories->setCurrentRow(0); window.grab().save(QStringLiteral("system-settings-preview.png"));
        QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": settings categories, navigation and Breeze main window\n"; app.exit(ok ? 0 : 1);
    });
    return app.exec();
}
