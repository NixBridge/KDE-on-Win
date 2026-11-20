// SPDX-License-Identifier: GPL-3.0-or-later
#include "localization.h"
#include <QApplication>
#include <QMap>
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
#include "desktop-actions.h"
#include "system-theme.h"
#include "diagnostics.h"
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QShortcut>
#include <QScrollBar>
#include <QToolButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QPainter>
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
    window.resize(960, 640);
    auto* central = new QWidget(&window);
    auto* layout = new QHBoxLayout(central);
    window.setCentralWidget(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* sidebar = new QWidget(central);
    sidebar->setFixedWidth(244);
    auto* navigation = new QVBoxLayout(sidebar);
    navigation->setContentsMargins(10, 10, 10, 10);
    auto* search = new QLineEdit(sidebar);
    search->setPlaceholderText(QObject::tr("Search settings…"));
    search->setAccessibleName(search->placeholderText());
    search->setClearButtonEnabled(true);
    search->addAction(QIcon::fromTheme(QStringLiteral("edit-find")), QLineEdit::LeadingPosition);
    navigation->addWidget(search);
    auto* overview = new QToolButton(sidebar);
    overview->setText(QObject::tr("Quick Settings"));
    overview->setIcon(QIcon::fromTheme(QStringLiteral("go-home")));
    overview->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    overview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    navigation->addWidget(overview);
    auto* categories = new QListWidget(sidebar);
    categories->setFrameShape(QFrame::NoFrame);
    categories->setIconSize(QSize(24, 24));
    navigation->addWidget(categories, 1);
    auto* noMatches = new QLabel(QObject::tr("No matching settings"), sidebar);
    noMatches->setWordWrap(true);
    noMatches->hide();
    navigation->addWidget(noMatches);
    layout->addWidget(sidebar);
    auto* divider = new QFrame(central);
    divider->setFrameShape(QFrame::VLine);
    layout->addWidget(divider);
    auto* pages = new QStackedWidget(central);
    auto* pageScroll = new QScrollArea(central);
    pageScroll->setWidgetResizable(true);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidget(pages);
    layout->addWidget(pageScroll, 1);
    auto page = [&](const QString& name, const QString& icon) {
        categories->addItem(
            new QListWidgetItem(QIcon(KIconLoader::global()->loadIcon(icon, KIconLoader::Desktop, 24)), name));
        categories->item(categories->count() - 1)->setSizeHint(QSize(0, 36));
        auto* widget = new QWidget(pages);
        auto* contents = new QVBoxLayout(widget);
        contents->setContentsMargins(24, 18, 24, 18);
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
    auto* followTheme = new QCheckBox(QObject::tr("Follow Windows light/dark mode (Breeze only)"));
    followTheme->setChecked(settings.value("followWindowsTheme", false).toBool());
    appearance->addWidget(followTheme);
    auto* themeHint = new QLabel(QObject::tr(
        "Manual theme selection disables following. Custom KDE themes and high-contrast mode are left unchanged."));
    themeHint->setWordWrap(true);
    appearance->addWidget(themeHint);
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
    QObject::connect(theme, &QComboBox::currentIndexChanged, &window, [&] {
        followTheme->setChecked(false);
        settings.setValue("followWindowsTheme", false);
        save();
    });
    QObject::connect(followTheme, &QCheckBox::toggled, &window, [&](bool checked) {
        settings.setValue("followWindowsTheme", checked);
        settings.sync();
        if (app.arguments().contains("--ui-smoke-test"))
            return;
        if (syncWindowsTheme(settings, windowsDarkPreference(), currentThemeIsBreeze())) {
            const QSignalBlocker block(theme);
            theme->setCurrentIndex(settings.value("dark", false).toBool() ? 1 : 0);
            if (applyAppearance)
                applyAppearance();
        }
        if (checked
            && !QProcess::startDetached(
                QCoreApplication::applicationDirPath() + "/plasma-desktop-tools.exe", { "--daemon" }))
            QMessageBox::warning(&window, QObject::tr("Cannot start application"),
                QObject::tr("Could not start desktop tools for theme following."));
    });
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
        if (followTheme->isChecked() != settings.value("followWindowsTheme", false).toBool()) {
            QSignalBlocker block(followTheme);
            followTheme->setChecked(settings.value("followWindowsTheme", false).toBool());
        }
        if (theme->currentData().toBool() != settings.value("dark", false).toBool()) {
            QSignalBlocker block(theme);
            theme->setCurrentIndex(settings.value("dark", false).toBool() ? 1 : 0);
        }
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
    auto* desktopTools = new QPushButton(QObject::tr("Clipboard history and desktop shortcuts…"));
    support->addWidget(desktopTools);
    QObject::connect(desktopTools, &QPushButton::clicked, &window, [&] {
        if (!QProcess::startDetached(app.applicationDirPath() + "/plasma-desktop-tools.exe", { "--settings" }))
            QMessageBox::warning(
                &window, QObject::tr("Cannot start application"), QObject::tr("Desktop tools could not be started."));
    });
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
        const auto error = startDesktopAction("logs");
        if (!error.isEmpty())
            QMessageBox::warning(&window, QObject::tr("Cannot start application"), error);
    });
    auto* exportDiagnostics = new QPushButton(QObject::tr("Preview and export diagnostics…"));
    support->addWidget(exportDiagnostics);
    exportDiagnostics->setObjectName("ExportDiagnostics");
    QObject::connect(exportDiagnostics, &QPushButton::clicked, &window, [&] {
        const auto json = QJsonDocument(diagnosticReport(app.applicationDirPath(), appearanceSettingsPath(),
                                            QFileInfo(appearanceSettingsPath()).absolutePath() + "/logs"))
                              .toJson(QJsonDocument::Indented);
        QDialog preview(&window);
        preview.setWindowTitle(QObject::tr("Review diagnostics before sharing"));
        preview.resize(700, 500);
        auto* contents = new QVBoxLayout(&preview);
        auto* hint = new QLabel(QObject::tr(
            "No automatic upload. Raw logs and personal paths are excluded; review this report before saving."));
        hint->setWordWrap(true);
        contents->addWidget(hint);
        auto* text = new QPlainTextEdit(QString::fromUtf8(json));
        text->setReadOnly(true);
        contents->addWidget(text);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        contents->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &preview, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &preview, &QDialog::reject);
        if (preview.exec() != QDialog::Accepted)
            return;
        const auto path = QFileDialog::getSaveFileName(&window, QObject::tr("Save diagnostics"),
            "kde-on-win-diagnostics.json", QObject::tr("JSON files (*.json)"));
        if (path.isEmpty())
            return;
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(json) != json.size() || !output.commit())
            QMessageBox::warning(&window, QObject::tr("Cannot save diagnostics"), output.errorString());
        else
            window.statusBar()->showMessage(QObject::tr("Diagnostics saved. Share only after reviewing."), 5000);
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
    auto* home = new QWidget(pages);
    auto* quickSettings = new QVBoxLayout(home);
    quickSettings->setContentsMargins(24, 18, 24, 18);
    quickSettings->addWidget(new SettingsHeading(overview->text(), home));
    auto* themeOptions = new QHBoxLayout;
    auto* themeButtons = new QButtonGroup(home);
    for (int index = 0; index < theme->count(); ++index) {
        const bool dark = theme->itemData(index).toBool();
        QPixmap preview(160, 84);
        preview.fill(dark ? QColor(35, 38, 41) : QColor(239, 240, 241));
        QPainter painter(&preview);
        painter.fillRect(16, 13, 128, 58, dark ? QColor(49, 54, 59) : Qt::white);
        painter.fillRect(16, 13, 128, 13, dark ? QColor(65, 70, 75) : QColor(218, 221, 224));
        painter.fillRect(27, 35, 30, 25, QColor(61, 174, 233));
        painter.fillRect(68, 38, 64, 4, dark ? QColor(216, 220, 224) : QColor(85, 92, 100));
        painter.fillRect(68, 50, 45, 4, dark ? QColor(162, 172, 181) : QColor(131, 140, 150));
        painter.end();
        auto* option = new QToolButton(home);
        option->setText(theme->itemText(index));
        option->setIcon(QIcon(preview));
        option->setIconSize(preview.size());
        option->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        option->setCheckable(true);
        option->setChecked(index == theme->currentIndex());
        themeButtons->addButton(option, index);
        themeOptions->addWidget(option);
    }
    themeOptions->addStretch();
    quickSettings->addLayout(themeOptions);
    QObject::connect(themeButtons, &QButtonGroup::idClicked, theme, &QComboBox::setCurrentIndex);
    // setCurrentIndex can also be called while the combo box signals are blocked.
    QObject::connect(pages, &QStackedWidget::currentChanged, home, [=](int) {
        if (pages->currentWidget() == home)
            themeButtons->button(theme->currentIndex())->setChecked(true);
    });
    auto* shortcuts = new QGridLayout;
    int shortcutIndex = 0;
    for (const int row : {0, 2, 4, 5, 6}) {
        const auto* entry = categories->item(row);
        auto* shortcut = new QPushButton(entry->icon(), entry->text(), home);
        shortcut->setMinimumHeight(38);
        shortcuts->addWidget(shortcut, shortcutIndex / 2, shortcutIndex % 2);
        ++shortcutIndex;
        QObject::connect(shortcut, &QPushButton::clicked, &window, [=] {
            search->clear();
            categories->setCurrentRow(row);
        });
    }
    quickSettings->addLayout(shortcuts);
    quickSettings->addStretch();
    const int overviewIndex = pages->addWidget(home);
    QObject::connect(overview, &QToolButton::clicked, &window, [=] {
        search->clear();
        categories->setCurrentRow(-1);
        pages->setCurrentIndex(overviewIndex);
    });
    QObject::connect(categories, &QListWidget::currentRowChanged, pages, [=](int row) {
        if (row >= 0)
            pages->setCurrentIndex(row);
    });
    QObject::connect(pages, &QStackedWidget::currentChanged, pageScroll,
        [=] { pageScroll->verticalScrollBar()->setValue(0); });
    QStringList terms;
    for (int row = 0; row < categories->count(); ++row) {
        const auto* module = pages->widget(row);
        QStringList words { categories->item(row)->text() };
        for (const auto* label : module->findChildren<QLabel*>())
            words.append(label->text());
        for (const auto* button : module->findChildren<QAbstractButton*>())
            words.append(button->text());
        terms.append(words.join(u' '));
    }
    QObject::connect(search, &QLineEdit::textChanged, &window, [=](const QString& text) {
        const auto words = text.simplified().split(u' ', Qt::SkipEmptyParts);
        bool found = false;
        for (int row = 0; row < categories->count(); ++row) {
            bool matches = true;
            for (const auto& word : words)
                matches &= terms.at(row).contains(word, Qt::CaseInsensitive);
            categories->item(row)->setHidden(!matches);
            found |= matches;
        }
        noMatches->setVisible(!found);
    });
    QObject::connect(search, &QLineEdit::returnPressed, &window, [=] {
        for (int row = 0; row < categories->count(); ++row) {
            if (!categories->item(row)->isHidden()) {
                categories->setCurrentRow(row);
                categories->setFocus();
                break;
            }
        }
    });
    auto* findShortcut = new QShortcut(QKeySequence::Find, &window);
    QObject::connect(findShortcut, &QShortcut::activated, search, [=] {
        search->setFocus();
        search->selectAll();
    });
    pages->setCurrentIndex(overviewIndex);
    const QMap<QString, int> pageNames { { "appearance", 0 }, { "panel", 1 }, { "desktop", 2 }, { "start-menu", 3 },
        { "display", 4 }, { "sound", 5 }, { "network", 6 } };
    for (const auto& argument : app.arguments())
        if (argument.startsWith("--page=") && pageNames.contains(argument.mid(7)))
            categories->setCurrentRow(pageNames.value(argument.mid(7)));
    window.show();
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test")))
        QTimer::singleShot(500, &window, [&] {
            int requested = overviewIndex;
            for (const auto& argument : app.arguments())
                if (argument.startsWith("--page=") && pageNames.contains(argument.mid(7)))
                    requested = pageNames.value(argument.mid(7));
            bool ok = pages->currentIndex() == requested && pages->count() == 15 && categories->count() == 14
                && height->minimum() == 24 && iconSize->maximum() == 72;
            ok &= exportDiagnostics->isEnabled();
            const int initialTheme = theme->currentIndex();
            followTheme->setChecked(true);
            theme->setCurrentIndex(1 - initialTheme);
            ok &= !followTheme->isChecked() && !settings.value("followWindowsTheme").toBool();
            theme->setCurrentIndex(initialTheme);
            categories->setCurrentRow(3);
            ok &= pages->currentIndex() == 3;
            search->setText(QStringLiteral("no-settings-match-9cc1"));
            ok &= !noMatches->isHidden() && categories->item(3)->isHidden();
            search->setText(categories->item(3)->text());
            ok &= noMatches->isHidden() && !categories->item(3)->isHidden();
            overview->click();
            ok &= search->text().isEmpty() && pages->currentIndex() == overviewIndex;
            themeButtons->button(1 - initialTheme)->click();
            ok &= theme->currentIndex() == 1 - initialTheme && themeButtons->checkedId() == 1 - initialTheme;
            window.grab().save(QStringLiteral("system-settings-overview.png"));
            themeButtons->button(initialTheme)->click();
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
