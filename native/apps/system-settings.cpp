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
#include <QSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QLineEdit>
#include <QFileInfo>
#include <QFile>
#include <QStatusBar>
#include <QUuid>
#include <QScrollArea>
#include <KIconTheme>
#include <KIconLoader>
#include "appearance.h"

int main(int argc, char** argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) {
        const int id
            = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/segoeui.ttf"));
        if (id >= 0)
            app.setFont(QFont(QFontDatabase::applicationFontFamilies(id).value(0), 10));
    }
    app.setDesktopFileName(QStringLiteral("org.nixbridge.systemsettings"));
    app.setWindowIcon(
        QIcon(KIconLoader::global()->loadIcon(QStringLiteral("preferences-system"), KIconLoader::Desktop, 48)));
    QMainWindow window;
    window.setWindowTitle(QObject::tr("System Settings"));
    window.resize(880, 600);
    auto* central = new QWidget(&window);
    auto* layout = new QHBoxLayout(central);
    window.setCentralWidget(central);
    auto* categories = new QListWidget(central);
    categories->setMaximumWidth(220);
    layout->addWidget(categories);
    auto* pages = new QStackedWidget(central);
    auto *pageScroll = new QScrollArea(central);
    pageScroll->setWidgetResizable(true);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidget(pages);
    layout->addWidget(pageScroll, 1);
    auto page = [&](const QString& name, const QString& icon) {
        categories->addItem(
            new QListWidgetItem(QIcon(KIconLoader::global()->loadIcon(icon, KIconLoader::Desktop, 24)), name));
        auto* widget = new QWidget(pages);
        auto* contents = new QVBoxLayout(widget);
        auto* heading = new QLabel(name, widget);
        QFont font = heading->font();
        font.setPointSize(font.pointSize() + 6);
        heading->setFont(font);
        contents->addWidget(heading);
        pages->addWidget(widget);
        return contents;
    };
    auto* appearance = page(QObject::tr("Appearance"), QStringLiteral("preferences-desktop-theme"));
    auto* description
        = new QLabel(QObject::tr("Choose the appearance of KDE-on-Win applications and standard window titlebars."));
    description->setWordWrap(true);
    appearance->addWidget(description);
    QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
    auto* theme = new QComboBox;
    theme->addItem(QObject::tr("Breeze Light"), false);
    theme->addItem(QObject::tr("Breeze Dark"), true);
    theme->setCurrentIndex(settings.value(QStringLiteral("dark"), false).toBool() ? 1 : 0);
    appearance->addWidget(theme);
    auto* decoration = new QCheckBox(QObject::tr("Use Breeze titlebars on standard Windows windows"));
    decoration->setChecked(settings.value(QStringLiteral("decorateWindows"), true).toBool());
    appearance->addWidget(decoration);
    auto* scope = new QLabel(
        QObject::tr("Custom titlebars are skipped. Application content keeps the style supported by its own toolkit."));
    scope->setWordWrap(true);
    appearance->addWidget(scope);
    appearance->addStretch();
    auto* fontSize = new QSpinBox;
    fontSize->setRange(8, 20);
    fontSize->setValue(settings.value(QStringLiteral("fontSize"), 10).toInt());
    fontSize->setPrefix(QObject::tr("Application font: "));
    fontSize->setSuffix(QObject::tr(" pt"));
    appearance->insertWidget(appearance->count() - 1, fontSize);
    QObject::connect(fontSize, &QSpinBox::valueChanged, &window, [&](int size) {
        settings.setValue(QStringLiteral("fontSize"), size);
        settings.sync();
    });
    auto save = [&] {
        settings.setValue(QStringLiteral("dark"), theme->currentData());
        settings.setValue(QStringLiteral("decorateWindows"), decoration->isChecked());
        settings.sync();
    };
    QObject::connect(theme, &QComboBox::currentIndexChanged, &window, save);
    QObject::connect(decoration, &QCheckBox::toggled, &window, save);
    auto* panel = page(QObject::tr("Panel"), QStringLiteral("preferences-desktop-plasma"));
    auto* panelForm = new QFormLayout;
    panel->addLayout(panelForm);
    auto* height = new QSpinBox;
    height->setRange(24, 96);
    height->setSuffix(QObject::tr(" px"));
    height->setValue(settings.value(QStringLiteral("panelHeight"), 44).toInt());
    panelForm->addRow(QObject::tr("Height"), height);
    auto combo
        = [&](const QString& key, const QString& fallback, const QStringList& labels, const QStringList& values) {
              auto* box = new QComboBox;
              for (int i = 0; i < labels.size(); ++i)
                  box->addItem(labels[i], values[i]);
              box->setCurrentIndex(qMax(0, box->findData(settings.value(key, fallback))));
              return box;
          };
    auto* position = combo(QStringLiteral("panelLocation"), QStringLiteral("bottom"),
        { QObject::tr("Bottom"), QObject::tr("Top"), QObject::tr("Left"), QObject::tr("Right") },
        { "bottom", "top", "left", "right" });
    panelForm->addRow(QObject::tr("Position"), position);
    auto* hiding = combo(QStringLiteral("panelHiding"), QStringLiteral("none"),
        { QObject::tr("Always visible"), QObject::tr("Auto hide"), QObject::tr("Dodge windows") },
        { "none", "autohide", "dodgewindows" });
    panelForm->addRow(QObject::tr("Visibility"), hiding);
    auto* opacity = combo(QStringLiteral("panelOpacity"), QStringLiteral("opaque"),
        { QObject::tr("Opaque"), QObject::tr("Translucent"), QObject::tr("Adaptive") },
        { "opaque", "translucent", "adaptive" });
    panelForm->addRow(QObject::tr("Opacity"), opacity);
    auto* floating = new QCheckBox(QObject::tr("Floating panel"));
    floating->setChecked(settings.value(QStringLiteral("panelFloating"), false).toBool());
    panel->addWidget(floating);
    auto* apply = new QPushButton(QObject::tr("Apply to all panels"));
    panel->addWidget(apply);
    auto* panelStatus
        = new QLabel(QObject::tr("Changes are saved in Plasma. Later edits made in Plasma remain yours."));
    panelStatus->setWordWrap(true);
    panel->addWidget(panelStatus);
    panel->addStretch();
    bool waitingForApply = false;
    auto applyPending = [&] {
        settings.setValue(QStringLiteral("personalizationRevision"),
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("pendingPersonalization"), true);
        settings.sync();
        const bool started = QProcess::startDetached(
            QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-personalization.exe"), {});
        panelStatus->setText(started ? QObject::tr("Saved. Waiting for Plasma to apply preferences…")
                                     : QObject::tr("Could not start the personalization helper."));
        waitingForApply = started;
        window.statusBar()->showMessage(panelStatus->text());
    };
    QObject::connect(apply, &QPushButton::clicked, &window, [&] {
        settings.setValue(QStringLiteral("pendingPanel"), true);
        settings.setValue(QStringLiteral("panelHeight"), height->value());
        settings.setValue(QStringLiteral("panelLocation"), position->currentData());
        settings.setValue(QStringLiteral("panelHiding"), hiding->currentData());
        settings.setValue(QStringLiteral("panelOpacity"), opacity->currentData());
        settings.setValue(QStringLiteral("panelFloating"), floating->isChecked());
        applyPending();
    });
    auto* resetPanel = new QPushButton(QObject::tr("Reset panel defaults"));
    panel->insertWidget(panel->count() - 1, resetPanel);
    QObject::connect(resetPanel, &QPushButton::clicked, &window, [&] {
        height->setValue(44);
        position->setCurrentIndex(0);
        hiding->setCurrentIndex(0);
        opacity->setCurrentIndex(0);
        floating->setChecked(false);
        apply->click();
    });
    auto* desktop = page(QObject::tr("Desktop"), QStringLiteral("preferences-desktop-wallpaper"));
    auto* wallpaper = new QLineEdit(settings.value(QStringLiteral("wallpaper")).toString());
    wallpaper->setReadOnly(true);
    desktop->addWidget(wallpaper);
    auto* chooseWallpaper = new QPushButton(QObject::tr("Choose wallpaper…"));
    desktop->addWidget(chooseWallpaper);
    QObject::connect(chooseWallpaper, &QPushButton::clicked, &window, [&] {
        const QString file = QFileDialog::getOpenFileName(
            &window, QObject::tr("Choose Wallpaper"), {}, QObject::tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (file.isEmpty())
            return;
        wallpaper->setText(file);
        settings.setValue(QStringLiteral("wallpaper"), file);
        settings.setValue(QStringLiteral("pendingWallpaper"), true);
        applyPending();
    });
    auto* desktopHint = new QLabel(QObject::tr(
        "The image is applied to all Plasma desktops. Use Plasma edit mode for per-screen widgets and layouts."));
    desktopHint->setWordWrap(true);
    desktop->addWidget(desktopHint);
    desktop->addStretch();
    auto* menuPage = page(QObject::tr("Start Menu"), QStringLiteral("start-here-kde"));
    auto* menuForm = new QFormLayout;
    menuPage->addLayout(menuForm);
    auto* columns = new QSpinBox;
    columns->setRange(2, 5);
    columns->setValue(settings.value(QStringLiteral("menuColumns"), 3).toInt());
    menuForm->addRow(QObject::tr("Columns"), columns);
    auto* iconSize = new QSpinBox;
    iconSize->setRange(24, 72);
    iconSize->setSuffix(QObject::tr(" px"));
    iconSize->setValue(settings.value(QStringLiteral("menuIconSize"), 40).toInt());
    menuForm->addRow(QObject::tr("Icon size"), iconSize);
    QObject::connect(columns, &QSpinBox::valueChanged, &window, [&](int value) {
        settings.setValue(QStringLiteral("menuColumns"), value);
        settings.sync();
    });
    QObject::connect(iconSize, &QSpinBox::valueChanged, &window, [&](int value) {
        settings.setValue(QStringLiteral("menuIconSize"), value);
        settings.sync();
    });
    const QString categoriesPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/plasma/startmenu-categories.json");
    QFile categoriesFile(categoriesPath);
    QJsonObject categoryConfig;
    if (categoriesFile.open(QIODevice::ReadOnly))
        categoryConfig = QJsonDocument::fromJson(categoriesFile.readAll()).object();
    categoriesFile.close();
    QJsonArray folders = categoryConfig.value(QStringLiteral("sidebar")).toArray();
    auto* folderList = new QListWidget;
    menuPage->addWidget(new QLabel(QObject::tr("Sidebar folders")));
    menuPage->addWidget(folderList);
    auto refreshFolders = [&] {
        folderList->clear();
        for (const auto& value : folders)
            folderList->addItem(value.toObject().value(QStringLiteral("name")).toString());
    };
    refreshFolders();
    auto saveFolders = [&] {
        QDir().mkpath(QFileInfo(categoriesPath).absolutePath());
        categoryConfig.insert(QStringLiteral("sidebar"), folders);
        QSaveFile file(categoriesPath);
        const QByteArray data = QJsonDocument(categoryConfig).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            QMessageBox::warning(&window, QObject::tr("Cannot Save Menu"), file.errorString());
            return;
        }
        refreshFolders();
    };
    auto* addFolder = new QPushButton(QObject::tr("Add sidebar folder…"));
    auto* removeFolder = new QPushButton(QObject::tr("Remove from menu"));
    menuPage->addWidget(addFolder);
    menuPage->addWidget(removeFolder);
    QObject::connect(addFolder, &QPushButton::clicked, &window, [&] {
        const QString path = QFileDialog::getExistingDirectory(&window, QObject::tr("Choose Menu Folder"));
        if (path.isEmpty())
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(&window, QObject::tr("Menu Folder Name"), QObject::tr("Name:"),
            QLineEdit::Normal, QFileInfo(path).fileName(), &ok)
                                 .trimmed();
        if (!ok || name.isEmpty())
            return;
        for (const auto& value : folders)
            if (value.toObject().value(QStringLiteral("name")).toString() == name) {
                QMessageBox::warning(
                    &window, QObject::tr("Duplicate Name"), QObject::tr("Choose a unique sidebar name."));
                return;
            }
        folders.append(QJsonObject { { "name", name }, { "icon", "folder" }, { "dirs", QJsonArray { path } } });
        saveFolders();
    });
    QObject::connect(removeFolder, &QPushButton::clicked, &window, [&] {
        const int row = folderList->currentRow();
        if (row < 0)
            return;
        folders.removeAt(row);
        saveFolders();
    });
    QTimer statusTimer;
    QObject::connect(&statusTimer, &QTimer::timeout, &window, [&] {
        settings.sync();
        if (waitingForApply && !settings.value(QStringLiteral("pendingPersonalization"), false).toBool()) {
            panelStatus->setText(QObject::tr("Personalization applied."));
            window.statusBar()->showMessage(panelStatus->text(), 5000);
            waitingForApply = false;
        }
    });
    statusTimer.start(1000);
    struct Category {
        const char* name;
        const char* icon;
        const char* uri;
        const char* description;
    };
    const Category links[] = {
        { "Display", "preferences-desktop-display", "ms-settings:display",
            "Resolution, scaling, orientation and monitors" },
        { "Sound", "preferences-desktop-sound", "ms-settings:sound", "Windows audio devices and volume" },
        { "Network", "preferences-system-network", "ms-settings:network-status", "Windows network connections" },
        { "Bluetooth", "preferences-system-bluetooth", "ms-settings:bluetooth", "Pair and manage Windows devices" },
        { "Language & Input", "preferences-desktop-locale", "ms-settings:regionlanguage",
            "Languages, keyboards and input methods" },
        { "Notifications", "preferences-desktop-notification", "ms-settings:notifications",
            "Windows notification and banner preferences" },
        { "Accounts", "preferences-system-users", "ms-settings:yourinfo", "Windows user account settings" },
        { "Power", "preferences-system-power-management", "ms-settings:powersleep",
            "Power, sleep and battery settings" },
        { "Updates", "system-software-update", "ms-settings:windowsupdate", "Windows Update settings" },
    };
    for (const auto& entry : links) {
        auto* contents = page(QObject::tr(entry.name), QLatin1String(entry.icon));
        auto* label = new QLabel(QObject::tr(entry.description));
        label->setWordWrap(true);
        contents->addWidget(label);
        auto* button = new QPushButton(QObject::tr("Open Windows Settings"));
        contents->addWidget(button);
        const QUrl uri(QLatin1String(entry.uri));
        QObject::connect(button, &QPushButton::clicked, &window, [uri] { QDesktopServices::openUrl(uri); });
        contents->addStretch();
    }
    auto* support = page(QObject::tr("About & Diagnostics"), QStringLiteral("help-about"));
    auto* about = new QLabel(QObject::tr("KDE-on-Win native desktop integration. Qt %1. Settings and editor "
                                         "replacements are project-owned applications.")
            .arg(QString::fromLatin1(qVersion())));
    about->setWordWrap(true);
    support->addWidget(about);
    auto* openLogs = new QPushButton(QObject::tr("Open session logs"));
    support->addWidget(openLogs);
    QObject::connect(openLogs, &QPushButton::clicked, &window, [&] {
        QProcess::startDetached(QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-kde-launcher.exe"),
            { QStringLiteral("dolphin"), QFileInfo(appearanceSettingsPath()).absolutePath() });
    });
    auto* checks = new QPushButton(QObject::tr("Run runtime diagnostics"));
    support->addWidget(checks);
    QObject::connect(checks, &QPushButton::clicked, &window, [] {
        QProcess::startDetached(QStringLiteral("cmd.exe"),
            { QStringLiteral("/c"),
                QDir::toNativeSeparators(
                    QCoreApplication::applicationDirPath() + QStringLiteral("/../run-runtime-checks.cmd")) });
    });
    auto* help = new QPushButton(QObject::tr("Personalization help"));
    support->addWidget(help);
    QObject::connect(help, &QPushButton::clicked, &window, [] {
        QProcess::startDetached(QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-kde-launcher.exe"),
            { QStringLiteral("text-editor"),
                QCoreApplication::applicationDirPath() + QStringLiteral("/../docs/PERSONALIZATION.md") });
    });
    support->addStretch();
    QObject::connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    categories->setCurrentRow(0);
    window.show();
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test")))
        QTimer::singleShot(500, &window, [&] {
            bool ok = pages->count() == 14 && categories->count() == 14 && height->minimum() == 24
                && iconSize->maximum() == 72;
            categories->setCurrentRow(3);
            ok &= pages->currentIndex() == 3;
            ok &= window.findChild<QWidget*>(QStringLiteral("PlasmaBreezeTitleBar")) != nullptr;
            categories->setCurrentRow(0);
            window.grab().save(QStringLiteral("system-settings-preview.png"));
            QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                                << ": settings categories, navigation and Breeze main window\n";
            app.exit(ok ? 0 : 1);
        });
    return app.exec();
}
