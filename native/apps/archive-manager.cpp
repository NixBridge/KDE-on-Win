// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <KArchive>
#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KZip>
#include <KTar>
#include <K7Zip>
#include <KIconTheme>
#include <KIconLoader>
#include <windows.h>
#include <shlobj.h>
#include <memory>
struct Item {
    QString path;
    const KArchiveEntry* entry;
};
static bool safePart(const QString& part)
{
    if (part.isEmpty() || part == "." || part == ".." || part.endsWith(' ') || part.endsWith('.'))
        return false;
    static const QRegularExpression reserved(
        "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)", QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(part).hasMatch())
        return false;
    for (auto c : part)
        if (c.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(c))
            return false;
    return true;
}
static bool collect(
    const KArchiveDirectory* dir, const QString& prefix, QList<Item>& items, QString& error, int depth = 0)
{
    if (depth > 64) {
        error = "Archive nesting exceeds 64 levels.";
        return false;
    }
    for (const QString& name : dir->entries()) {
        const auto* entry = dir->entry(name);
        if (!safePart(name) || !entry || !entry->symLinkTarget().isEmpty()
            || (!entry->isDirectory() && !entry->isFile())) {
            error = "Unsafe archive entry: " + prefix + name;
            return false;
        }
        items.append({ prefix + name, entry });
        if (items.size() > 100000) {
            error = "Archive has too many entries.";
            return false;
        }
        if (entry->isDirectory()
            && !collect(static_cast<const KArchiveDirectory*>(entry), prefix + name + '/', items, error, depth + 1))
            return false;
    }
    return true;
}
static bool plainPath(const QString& path)
{
    const auto native = QDir::toNativeSeparators(path);
    DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}
static int worker(const QStringList& args)
{
    const QString path = QFileInfo(args.value(2)).absoluteFilePath();
    std::unique_ptr<KArchive> archive;
    if (path.endsWith(".zip", Qt::CaseInsensitive))
        archive = std::make_unique<KZip>(path);
    else if (path.endsWith(".7z", Qt::CaseInsensitive))
        archive = std::make_unique<K7Zip>(path);
    else
        archive = std::make_unique<KTar>(path);
    QJsonObject response;
    QList<Item> items;
    QString error;
    if (!archive->open(QIODevice::ReadOnly) || !archive->directory())
        error = "Cannot read this archive (unsupported, encrypted or damaged).";
    else
        collect(archive->directory(), {}, items, error);
    qint64 total = 0;
    for (const auto& item : items)
        if (item.entry->isFile()) {
            qint64 size = static_cast<const KArchiveFile*>(item.entry)->size();
            if (size < 0 || size > 64LL * 1024 * 1024 * 1024 - total) {
                error = "Archive exceeds the 64 GiB extraction limit.";
                break;
            }
            total += size;
        }
    if (error.isEmpty() && args.value(1) == "--extract") {
        const QString parent = QFileInfo(args.value(3)).canonicalFilePath();
        if (parent.isEmpty() || !QFileInfo(parent).isDir() || !plainPath(parent))
            error = "Choose an existing ordinary destination directory.";
        else {
            QTemporaryDir destination(parent + "/KDE-extracted-XXXXXX");
            if (!destination.isValid())
                error = "Cannot create extraction directory.";
            else
                for (const auto& item : items) {
                    const QString target = destination.path() + '/' + item.path;
                    const QString parentPath = QFileInfo(target).absolutePath();
                    QString walk = destination.path();
                    for (const auto& part :
                        QDir(destination.path()).relativeFilePath(parentPath).split('/', Qt::SkipEmptyParts)) {
                        if (part == ".")
                            continue;
                        walk += '/' + part;
                        if (!QDir().mkpath(walk) || !plainPath(walk)) {
                            error = "Unsafe destination path.";
                            break;
                        }
                    }
                    if (!error.isEmpty())
                        break;
                    if (item.entry->isDirectory()) {
                        if (!QDir().mkpath(target) || !plainPath(target)) {
                            error = "Cannot create directory.";
                            break;
                        }
                        continue;
                    }
                    if (QFileInfo::exists(target)) {
                        error = "Duplicate archive path: " + item.path;
                        break;
                    }
                    auto* file = static_cast<const KArchiveFile*>(item.entry);
                    std::unique_ptr<QIODevice> input(file->createDevice());
                    QSaveFile output(target);
                    output.setDirectWriteFallback(false);
                    if (!input || (!input->isOpen() && !input->open(QIODevice::ReadOnly))
                        || !output.open(QIODevice::WriteOnly)) {
                        error = "Cannot extract " + item.path;
                        break;
                    }
                    qint64 copied = 0;
                    while (copied < file->size()) {
                        QByteArray bytes = input->read(1024 * 1024);
                        if (bytes.isEmpty() || copied > file->size() - bytes.size()
                            || output.write(bytes) != bytes.size()) {
                            error = "Archive read/write failed: " + item.path;
                            break;
                        }
                        copied += bytes.size();
                    }
                    if (!error.isEmpty() || copied != file->size() || !output.commit()) {
                        error = "Extraction failed: " + item.path;
                        break;
                    }
                }
            if (error.isEmpty()) {
                response["destination"] = destination.path();
                destination.setAutoRemove(false);
            }
        }
    } else if (error.isEmpty()) {
        QJsonArray entries;
        for (const auto& item : items) {
            QJsonObject o { { "path", item.path }, { "directory", item.entry->isDirectory() } };
            if (item.entry->isFile())
                o["size"] = static_cast<const KArchiveFile*>(item.entry)->size();
            entries.append(o);
        }
        response["entries"] = entries;
    }
    response["error"] = error;
    // JSON is already UTF-8. QTextStream's QByteArray insertion uses the
    // system codec on Windows and corrupts Chinese archive names.
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    output.write(QJsonDocument(response).toJson(QJsonDocument::Compact));
    output.write("\n");
    output.flush();
    return error.isEmpty() ? 0 : 1;
}
int main(int argc, char** argv)
{
    if (argc > 1 && (QString::fromLocal8Bit(argv[1]) == "--list" || QString::fromLocal8Bit(argv[1]) == "--extract")) {
        QCoreApplication app(argc, argv);
        return worker(app.arguments());
    }
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    if (app.arguments().contains("--choose-default")) {
        const QString file
            = QFileDialog::getOpenFileName(nullptr, QObject::tr("Choose an archive to set its default application"), {},
                "Archives (*.zip *.7z *.tar *.tgz *.tbz2 *.txz)");
        if (file.isEmpty())
            return 0;
        const QString native = QDir::toNativeSeparators(file);
        OPENASINFO info {};
        info.pcszFile = reinterpret_cast<LPCWSTR>(native.utf16());
        info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT;
        const HRESULT hr = SHOpenWithDialog(nullptr, &info);
        return FAILED(hr) ? 1 : 0;
    }
    app.setDesktopFileName("org.nixbridge.archive");
    QMainWindow window;
    window.setWindowTitle(QObject::tr("Archive Manager"));
    window.setWindowIcon(QIcon(KIconLoader::global()->loadIcon("utilities-file-archiver", KIconLoader::Desktop, 48)));
    window.resize(850, 550);
    auto* table = new QTableWidget(0, 2, &window);
    table->setHorizontalHeaderLabels({ QObject::tr("Name"), QObject::tr("Size") });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    window.setCentralWidget(table);
    auto* bar = window.addToolBar(QObject::tr("Archive"));
    auto* open = bar->addAction(QObject::tr("Open…"));
    auto* extract = bar->addAction(QObject::tr("Extract…"));
    extract->setEnabled(false);
    auto* cancel = bar->addAction(QObject::tr("Cancel"));
    cancel->setEnabled(false);
    QString path;
    QProcess process(&window);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &process, [&] {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(1000);
        }
    });
    auto start = [&](const QStringList& args) {
        open->setEnabled(false);
        extract->setEnabled(false);
        cancel->setEnabled(true);
        window.statusBar()->showMessage(QObject::tr("Reading archive…"));
        process.start(app.applicationFilePath(), args);
    };
    QObject::connect(&process, &QProcess::finished, &window, [&](int code, QProcess::ExitStatus kind) {
        open->setEnabled(true);
        cancel->setEnabled(false);
        const auto obj = QJsonDocument::fromJson(process.readAllStandardOutput().trimmed()).object();
        if (kind != QProcess::NormalExit || code || obj.isEmpty() || !obj["error"].toString().isEmpty()) {
            window.statusBar()->showMessage(QObject::tr("Operation failed or cancelled."));
            if (kind == QProcess::NormalExit)
                QMessageBox::warning(&window, QObject::tr("Archive error"), obj["error"].toString());
            return;
        }
        extract->setEnabled(true);
        if (obj.contains("destination")) {
            const QString dest = obj["destination"].toString();
            window.statusBar()->showMessage(QObject::tr("Extracted to %1").arg(dest));
            QProcess::startDetached(app.applicationDirPath() + "/plasma-kde-launcher.exe", { "dolphin", dest });
        } else {
            const auto entries = obj["entries"].toArray();
            table->setRowCount(entries.size());
            int row = 0;
            for (auto value : entries) {
                const auto entry = value.toObject();
                table->setItem(row, 0, new QTableWidgetItem(entry["path"].toString()));
                table->setItem(row++, 1,
                    new QTableWidgetItem(entry["directory"].toBool() ? QObject::tr("Folder")
                                                                     : QString::number(entry["size"].toInteger())));
            }
            window.statusBar()->showMessage(QObject::tr("%1 entries").arg(entries.size()));
        }
    });
    QObject::connect(&process, &QProcess::errorOccurred, &window, [&](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            open->setEnabled(true);
            cancel->setEnabled(false);
            QMessageBox::warning(&window, QObject::tr("Archive error"), process.errorString());
        }
    });
    auto load = [&](const QString& file) {
        if (file.isEmpty())
            return;
        path = QFileInfo(file).absoluteFilePath();
        window.setWindowTitle(QFileInfo(path).fileName() + " — " + QObject::tr("Archive Manager"));
        table->setRowCount(0);
        start({ "--list", path });
    };
    QObject::connect(open, &QAction::triggered, &window, [&] {
        load(QFileDialog::getOpenFileName(&window, QObject::tr("Open archive"), {},
            "Archives (*.zip *.7z *.tar *.tar.gz *.tgz *.tar.bz2 *.tbz2 *.tar.xz *.txz *.tar.zst);;All files (*)"));
    });
    QObject::connect(extract, &QAction::triggered, &window, [&] {
        auto destination = QFileDialog::getExistingDirectory(&window, QObject::tr("Extract into a new folder under…"));
        if (!destination.isEmpty())
            start({ "--extract", path, destination });
    });
    QObject::connect(cancel, &QAction::triggered, &process, &QProcess::kill);
    window.show();
    if (app.arguments().contains("--ui-smoke-test"))
        QTimer::singleShot(300, &app, [&] {
            bool ok = window.findChild<QWidget*>("PlasmaBreezeTitleBar") && table->columnCount() == 2;
            QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": archive UI\n";
            app.exit(ok ? 0 : 1);
        });
    else if (app.arguments().size() > 1)
        load(app.arguments().at(1));
    return app.exec();
}
