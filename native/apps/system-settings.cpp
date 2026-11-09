// SPDX-License-Identifier: GPL-3.0-or-later
#include "localization.h"
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
#include <QCloseEvent>
#include <QFontComboBox>
#include <QColorDialog>
#include <KIconTheme>
#include <KIconLoader>
#include "appearance.h"
#include "system-controls.h"
#include <windows.h>
#include <shellapi.h>

class SettingsHeading final : public QLabel {
public:
    SettingsHeading(const QString& text, QWidget* parent)
        : QLabel(text, parent)
    {
        refresh();
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == qApp && event->type() == QEvent::ApplicationFontChange)
            refresh();
        return QLabel::eventFilter(watched, event);
    }

private:
    void refresh()
    {
        QFont font = qApp->font();
        font.setPointSize(qMax(8, font.pointSize()) + 6);
        setFont(font);
    }
};
class SettingsWindow final : public QMainWindow {
    void closeEvent(QCloseEvent* event) override
    {
        if (property("updatesRunning").toBool()) {
            QMessageBox::information(this, QObject::tr("Updates are running"),
                QObject::tr("Wait for the selected updates to finish before closing Settings."));
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }
};
int main(int argc, char** argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    installProjectTranslations(&app);
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        const int id
            = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/segoeui.ttf"));
        if (id >= 0)
            app.setFont(QFont(QFontDatabase::applicationFontFamilies(id).value(0), 10));
    }
    app.setDesktopFileName(QStringLiteral("org.nixbridge.systemsettings"));
    app.setWindowIcon(
        QIcon(KIconLoader::global()->loadIcon(QStringLiteral("preferences-system"), KIconLoader::Desktop, 48)));
    SettingsWindow window;
    window.setWindowTitle(QObject::tr("System Settings"));
    window.resize(880, 600);
    auto* central = new QWidget(&window);
    auto* layout = new QHBoxLayout(central);
    window.setCentralWidget(central);
    auto* categories = new QListWidget(central);
    categories->setMaximumWidth(220);
    layout->addWidget(categories);
    auto* pages = new QStackedWidget(central);
    auto* pageScroll = new QScrollArea(central);
    pageScroll->setWidgetResizable(true);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidget(pages);
    layout->addWidget(pageScroll, 1);
    auto page = [&](const QString& name, const QString& icon) {
        categories->addItem(
            new QListWidgetItem(QIcon(KIconLoader::global()->loadIcon(icon, KIconLoader::Desktop, 24)), name));
        auto* widget = new QWidget(pages);
        auto* contents = new QVBoxLayout(widget);
        auto* heading = new SettingsHeading(name, widget);
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
    auto* nativeControls = new QCheckBox(QObject::tr("Style classic Windows controls (experimental)"));
    nativeControls->setChecked(settings.value("nativeControls", true).toBool());
    appearance->addWidget(nativeControls);
    QObject::connect(nativeControls, &QCheckBox::toggled, &window, [&](bool enabled) {
        settings.setValue("nativeControls", enabled);
        settings.sync();
    });
    auto* startControls = new QPushButton(QObject::tr("Start Windows control styling"));
    auto* stopControls = new QPushButton(QObject::tr("Stop Windows control styling"));
    appearance->addWidget(startControls);
    appearance->addWidget(stopControls);
    auto controlService = [](bool stop) {
        for (const auto& name : { QStringLiteral("/kde-style-host.exe"), QStringLiteral("/x86/kde-style-host.exe") }) {
            const auto path = QCoreApplication::applicationDirPath() + name;
            if (QFileInfo::exists(path))
                QProcess::startDetached(path, stop ? QStringList { "--stop" } : QStringList {});
        }
    };
    QObject::connect(startControls, &QPushButton::clicked, &window, [=] { controlService(false); });
    QObject::connect(stopControls, &QPushButton::clicked, &window, [=] { controlService(true); });
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
    std::function<void()> applyAppearance;
    QObject::connect(fontSize, &QSpinBox::valueChanged, &window, [&](int size) {
        settings.setValue(QStringLiteral("fontSize"), size);
        settings.sync();
        if (applyAppearance)
            applyAppearance();
    });
    auto save = [&] {
        settings.setValue(QStringLiteral("dark"), theme->currentData());
        settings.setValue(QStringLiteral("decorateWindows"), decoration->isChecked());
        settings.sync();
        if (applyAppearance)
            applyAppearance();
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
        if (app.arguments().contains(QStringLiteral("--ui-smoke-test")))
            return;
        settings.remove("personalizationError");
        settings.setValue(
            QStringLiteral("personalizationRevision"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("pendingPersonalization"), true);
        settings.sync();
        const bool started = QProcess::startDetached(
            QCoreApplication::applicationDirPath() + QStringLiteral("/plasma-personalization.exe"), {});
        panelStatus->setText(started ? QObject::tr("Saved. Waiting for Plasma to apply preferences…")
                                     : QObject::tr("Could not start the personalization helper."));
        waitingForApply = started;
        window.statusBar()->showMessage(panelStatus->text());
    };
    applyAppearance = [&] {
        settings.setValue("pendingAppearance", true);
        applyPending();
    };
    auto* family = new QFontComboBox;
    family->setCurrentFont(QFont(settings.value("fontFamily", defaultProjectFontFamily()).toString()));
    appearance->insertWidget(appearance->count() - 1, new QLabel(QObject::tr("Application font family")));
    appearance->insertWidget(appearance->count() - 1, family);
    QObject::connect(family, &QFontComboBox::currentFontChanged, &window, [&](const QFont& font) {
        settings.setValue("fontFamily", font.family());
        applyAppearance();
    });
    auto* accent = new QPushButton(QObject::tr("Choose accent color…"));
    appearance->insertWidget(appearance->count() - 1, accent);
    QObject::connect(accent, &QPushButton::clicked, &window, [&] {
        const QColor chosen = QColorDialog::getColor(
            QColor(settings.value("accentColor", "#3daee9").toString()), &window, QObject::tr("Accent color"));
        if (chosen.isValid()) {
            settings.setValue("accentColor", chosen.name());
            applyAppearance();
        }
    });
    auto* resetAccent = new QPushButton(QObject::tr("Use default accent color"));
    appearance->insertWidget(appearance->count() - 1, resetAccent);
    QObject::connect(resetAccent, &QPushButton::clicked, &window, [&] {
        settings.remove("accentColor");
        applyAppearance();
    });
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
        if (waitingForApply && !settings.value("personalizationError").toString().isEmpty())
            window.statusBar()->showMessage(
                QObject::tr("Could not apply preferences: %1").arg(settings.value("personalizationError").toString()));
        if (waitingForApply && !settings.value(QStringLiteral("pendingPersonalization"), false).toBool()) {
            panelStatus->setText(QObject::tr("Personalization applied."));
            window.statusBar()->showMessage(panelStatus->text(), 5000);
            waitingForApply = false;
        }
    });
    statusTimer.start(1000);
    const char* names[] = { QT_TRANSLATE_NOOP("QObject", "Display"), QT_TRANSLATE_NOOP("QObject", "Sound"),
        QT_TRANSLATE_NOOP("QObject", "Network"), QT_TRANSLATE_NOOP("QObject", "Bluetooth"),
        QT_TRANSLATE_NOOP("QObject", "Language & Input"), QT_TRANSLATE_NOOP("QObject", "Notifications"),
        QT_TRANSLATE_NOOP("QObject", "Accounts"), QT_TRANSLATE_NOOP("QObject", "Power"),
        QT_TRANSLATE_NOOP("QObject", "Updates") };
    const char* icons[] = { "preferences-desktop-display", "preferences-desktop-sound", "preferences-system-network",
        "preferences-system-bluetooth", "preferences-desktop-locale", "preferences-desktop-notification",
        "preferences-system-users", "preferences-system-power-management", "system-software-update" };
    for (int i = 0; i < 9; ++i)
        addDirectSystemControls(page(QObject::tr(names[i]), QLatin1String(icons[i])), i);
    auto* support = page(QObject::tr("About & Diagnostics"), QStringLiteral("help-about"));
    auto* associations = new QPushButton(QObject::tr("Use Dolphin for folders and KDE for archives"));
    support->addWidget(associations);
    QObject::connect(associations, &QPushButton::clicked, &window, [] {
        QProcess::startDetached(QStringLiteral("cmd.exe"),
            { "/c",
                QDir::toNativeSeparators(
                    QCoreApplication::applicationDirPath() + "/../install-file-associations.cmd") });
    });
    auto* linkChooser = new QPushButton(QObject::tr("Configure KDE link chooser…"));
    support->addWidget(linkChooser);
    QObject::connect(linkChooser, &QPushButton::clicked, &window, [] {
        QProcess::startDetached(
            QCoreApplication::applicationDirPath() + "/plasma-open-with.exe", { "--configure-defaults" });
    });
    auto* archiveDefault = new QPushButton(QObject::tr("Choose protected archive default (Open With)…"));
    support->addWidget(archiveDefault);
    QObject::connect(archiveDefault, &QPushButton::clicked, &window, [] {
        QProcess::startDetached(
            QCoreApplication::applicationDirPath() + "/plasma-archive-manager.exe", { "--choose-default" });
    });
    auto* about = new QLabel(QObject::tr("KDE-on-Win native desktop integration. Qt %1. Settings and editor "
                                         "replacements are project-owned applications.")
            .arg(QString::fromLatin1(qVersion())));
    about->setWordWrap(true);
    support->addWidget(about);
    auto* admin = new QPushButton(QObject::tr("Open System Settings as administrator…"));
    support->addWidget(admin);
    QObject::connect(admin, &QPushButton::clicked, &window, [] {
        const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        ShellExecuteW(
            nullptr, L"runas", reinterpret_cast<LPCWSTR>(executable.utf16()), nullptr, nullptr, SW_SHOWNORMAL);
    });
    auto* repository = new QLabel(QStringLiteral(
        "<a href=\"https://github.com/NixBridge/KDE-on-Win\">https://github.com/NixBridge/KDE-on-Win</a>"));
    repository->setOpenExternalLinks(true);
    support->addWidget(repository);

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
                QCoreApplication::applicationDirPath()
                    + (projectLanguage().startsWith("zh") ? QStringLiteral("/../docs/FIX13.md")
                                                          : QStringLiteral("/../docs/PERSONALIZATION.md")) });
    });
    auto* language = new QComboBox;
    language->addItem(QObject::tr("Follow system language"), "system");
    language->addItem(QString::fromUtf8("简体中文"), "zh_CN");
    language->addItem(QStringLiteral("English"), "en_US");
    language->setCurrentIndex(qMax(0, language->findData(settings.value("language", "system"))));
    support->addWidget(language);
    auto* applyLanguage = new QPushButton(QObject::tr("Apply language and reopen Settings"));
    support->addWidget(applyLanguage);
    QObject::connect(applyLanguage, &QPushButton::clicked, &window, [&] {
        settings.setValue("language", language->currentData());
        settings.sync();
        if (window.property("updatesRunning").toBool())
            return;
        if (QProcess::startDetached(app.applicationFilePath(), {}))
            window.close();
    });
    support->addStretch();
    QObject::connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    categories->setCurrentRow(0);
    if (app.arguments().contains(QStringLiteral("--page=network")))
        categories->setCurrentRow(6);
    window.show();
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test")))
        QTimer::singleShot(500, &window, [&] {
            bool ok = pages->count() == 14 && categories->count() == 14 && height->minimum() == 24
                && iconSize->maximum() == 72;
            categories->setCurrentRow(3);
            ok &= pages->currentIndex() == 3;
            ok &= window.findChild<QWidget*>(QStringLiteral("PlasmaBreezeTitleBar")) != nullptr;
            categories->setCurrentRow(0);
            const QFont original = app.font();
            QFont larger = original;
            larger.setPointSize(14);
            app.setFont(larger);
            const auto headings = window.findChildren<QLabel*>();
            for (auto* label : headings)
                if (auto* heading = dynamic_cast<SettingsHeading*>(label))
                    ok &= heading->font().pointSize() == 20;
            app.setFont(original);
            if (app.arguments().contains("--expect-chinese"))
                ok &= window.windowTitle() == QString::fromUtf8("系统设置")
                    && categories->item(0)->text() == QString::fromUtf8("外观")
                    && theme->itemText(1) == QString::fromUtf8("Breeze 深色")
                    && QFontMetrics(window.font()).inFontUcs4(0x7cfb);
            window.grab().save(QStringLiteral("system-settings-preview.png"));
            QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                                << ": settings categories, navigation and Breeze main window\n";
            app.exit(ok ? 0 : 1);
        });
    return app.exec();
}
