// SPDX-License-Identifier: GPL-3.0-or-later
#include "project-about.h"
#include "localization.h"
#include <QApplication>
#include <QMainWindow>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QMessageBox>
#include <QAction>
#include <QHash>
#include <QThread>
#include <QTextStream>
#include <QProcess>
#include <QFontDatabase>
#include <KIconTheme>
#include <KIconLoader>
#include "windowsiconmap.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

static quint64 ticks(FILETIME value) { return (quint64(value.dwHighDateTime) << 32) | value.dwLowDateTime; }
struct Process {
    DWORD pid;
    QString name, path;
    quint64 created = 0, cpuTicks = 0, memory = 0;
    double cpu = -1;
    bool memoryKnown = false;
};
class Processes final : public QAbstractTableModel {
    Q_OBJECT

public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows.size(); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 4; }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
            return QStringList { tr("Process"), tr("PID"), tr("CPU %"), tr("Memory (MiB)") }.value(section);
        return {};
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= rows.size())
            return {};
        const auto& p = rows[index.row()];
        if (role == Qt::ToolTipRole)
            return p.path.isEmpty() ? p.name : p.path;
        if (role == Qt::TextAlignmentRole)
            return int(index.column() ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
        if (role == Qt::DecorationRole && index.column() == 0) {
            const auto icon = breezeIconForWindowsProgram(p.name);
            return QIcon::fromTheme(icon.isEmpty() ? "application-x-executable" : icon);
        }
        if (role != Qt::DisplayRole && role != Qt::UserRole)
            return {};
        switch (index.column()) {
        case 0:
            return p.name;
        case 1:
            return qulonglong(p.pid);
        case 2:
            if (role == Qt::UserRole)
                return p.cpu;
            return p.cpu < 0 ? QStringLiteral("--") : QString::number(p.cpu, 'f', 1);
        case 3:
            if (role == Qt::UserRole)
                return p.memoryKnown ? double(p.memory) / 1048576.0 : -1.0;
            return p.memoryKnown ? QString::number(double(p.memory) / 1048576.0, 'f', 1) : QStringLiteral("--");
        }
        return {};
    }
    bool refresh()
    {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;
        QVector<Process> next;
        PROCESSENTRY32W entry {};
        entry.dwSize = sizeof(entry);
        const quint64 now = GetTickCount64();
        const double elapsed = (now - lastRefresh) * 10000.0;
        const DWORD processors = qMax<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
        if (Process32FirstW(snapshot, &entry))
            do {
                Process p;
                p.pid = entry.th32ProcessID;
                p.name = QString::fromWCharArray(entry.szExeFile);
                HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, p.pid);
                if (!handle)
                    handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid);
                if (handle) {
                    FILETIME created, exit, kernel, user;
                    if (GetProcessTimes(handle, &created, &exit, &kernel, &user)) {
                        p.created = ticks(created);
                        p.cpuTicks = ticks(kernel) + ticks(user);
                        const auto old = previous.constFind(p.pid);
                        if (old != previous.cend() && old->created == p.created && elapsed > 0
                            && p.cpuTicks >= old->cpuTicks)
                            p.cpu = qBound(0.0, (p.cpuTicks - old->cpuTicks) * 100.0 / elapsed / processors, 100.0);
                    }
                    PROCESS_MEMORY_COUNTERS_EX memory {};
                    memory.cb = sizeof(memory);
                    if (GetProcessMemoryInfo(
                            handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))) {
                        p.memory = memory.WorkingSetSize;
                        p.memoryKnown = true;
                    }
                    wchar_t path[32768];
                    DWORD length = 32768;
                    if (QueryFullProcessImageNameW(handle, 0, path, &length))
                        p.path = QString::fromWCharArray(path, int(length));
                    CloseHandle(handle);
                }
                next.append(p);
            } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
        previous.clear();
        for (const auto& p : next)
            previous.insert(p.pid, p);
        lastRefresh = now;
        beginResetModel();
        rows = std::move(next);
        endResetModel();
        return true;
    }
    QVector<Process> rows;

private:
    QHash<DWORD, Process> previous;
    quint64 lastRefresh = GetTickCount64();
};

static QString stopProcess(const Process& process)
{
    if (process.pid <= 4 || process.pid == GetCurrentProcessId() || !process.created)
        return QObject::tr("This process cannot be ended here.");
    const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, process.pid);
    if (!handle)
        return QObject::tr("Windows denied access, or the process has already exited.");
    FILETIME created, exit, kernel, user;
    BOOL critical = TRUE;
    QString error;
    if (!GetProcessTimes(handle, &created, &exit, &kernel, &user) || ticks(created) != process.created)
        error = QObject::tr("The selected process has exited or changed. Refresh and try again.");
    else if (!IsProcessCritical(handle, &critical) || critical)
        error = QObject::tr("Protected or critical Windows processes cannot be ended here.");
    else if (!TerminateProcess(handle, 1))
        error = QObject::tr("Windows could not end the selected process.");
    CloseHandle(handle);
    return error;
}

int main(int argc, char** argv)
{
    KIconTheme::initTheme();
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    QApplication app(argc, argv);
    installProjectTranslations(&app);
    app.setApplicationName("plasma-process-monitor");
    app.setDesktopFileName("org.nixbridge.processmonitor");
    app.setWindowIcon(
        QIcon(KIconLoader::global()->loadIcon(QStringLiteral("utilities-system-monitor"), KIconLoader::Desktop, 48)));
    if (app.arguments().contains("--ui-smoke-test")) {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        const int font = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
        if (font >= 0)
            app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).value(0), 10));
    }
    if (app.arguments().contains("--test-child")) {
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    Processes model;
    model.refresh();
    if (app.arguments().contains("--self-test")) {
        const auto pid = GetCurrentProcessId();
        bool found = false;
        for (const auto& p : model.rows)
            if (p.pid == pid)
                found = p.created != 0 && p.memoryKnown;
        QThread::msleep(120);
        model.refresh();
        bool sampled = false;
        for (const auto& p : model.rows)
            if (p.pid == pid)
                sampled = p.cpu >= 0;
        const bool blocked = !stopProcess(Process { pid }).isEmpty();
        QProcess child;
        child.start(app.applicationFilePath(), { "--test-child" });
        bool ended = false;
        if (child.waitForStarted(5000)) {
            model.refresh();
            for (const auto& process : model.rows)
                if (process.pid == child.processId()) {
                    ended = stopProcess(process).isEmpty();
                    break;
                }
            ended = ended && child.waitForFinished(5000);
        }
        if (child.state() != QProcess::NotRunning) {
            child.kill();
            child.waitForFinished(5000);
        }
        const bool ok = found && sampled && blocked && ended;
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": enumeration, identity, memory, CPU; self-protection=" << blocked
                            << " owned-child termination=" << ended << '\n';
        return ok ? 0 : 1;
    }
    QMainWindow window;
    addProjectAbout(&window);
    window.setWindowTitle(QObject::tr("Process Monitor"));
    window.resize(900, 600);
    auto* proxy = new QSortFilterProxyModel(&window);
    proxy->setSourceModel(&model);
    proxy->setSortRole(Qt::UserRole);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    auto* view = new QTableView(&window);
    view->setModel(proxy);
    view->setSortingEnabled(true);
    view->sortByColumn(2, Qt::DescendingOrder);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setAlternatingRowColors(true);
    view->setShowGrid(false);
    view->verticalHeader()->hide();
    view->horizontalHeader()->setStretchLastSection(false);
    view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    view->setColumnWidth(1, 90);
    view->setColumnWidth(2, 90);
    view->setColumnWidth(3, 145);
    window.setCentralWidget(view);
    auto* toolbar = window.addToolBar(QObject::tr("Processes"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto* search = new QLineEdit(toolbar);
    search->setPlaceholderText(QObject::tr("Search processes or PID"));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(280);
    toolbar->addWidget(search);
    QObject::connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    auto* end = toolbar->addAction(QIcon::fromTheme("process-stop"), QObject::tr("End Process"));
    end->setEnabled(false);
    QObject::connect(
        view->selectionModel(), &QItemSelectionModel::currentChanged, &window, [&](const QModelIndex& current) {
            const auto index = proxy->mapToSource(current);
            end->setEnabled(index.isValid() && model.rows[index.row()].pid > 4
                && model.rows[index.row()].pid != GetCurrentProcessId() && model.rows[index.row()].created != 0);
        });
    QObject::connect(end, &QAction::triggered, &window, [&] {
        const auto index = proxy->mapToSource(view->currentIndex());
        if (!index.isValid())
            return;
        const Process process = model.rows[index.row()];
        if (QMessageBox::question(&window, QObject::tr("End Process"),
                QObject::tr("End %1 (PID %2)? Unsaved work in that process may be lost.")
                    .arg(process.name)
                    .arg(process.pid),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
            != QMessageBox::Yes)
            return;
        const QString error = stopProcess(process);
        if (!error.isEmpty())
            QMessageBox::warning(&window, QObject::tr("Cannot End Process"), error);
        model.refresh();
    });
    auto refresh = [&] {
        DWORD selected = 0;
        quint64 identity = 0;
        const auto current = proxy->mapToSource(view->currentIndex());
        if (current.isValid()) {
            selected = model.rows[current.row()].pid;
            identity = model.rows[current.row()].created;
        }
        model.refresh();
        if (selected)
            for (int row = 0; row < proxy->rowCount(); ++row)
                if (proxy->data(proxy->index(row, 1)).toUInt() == selected
                    && model.rows[proxy->mapToSource(proxy->index(row, 0)).row()].created == identity) {
                    view->selectionModel()->setCurrentIndex(
                        proxy->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    break;
                }
        MEMORYSTATUSEX memory {};
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatusEx(&memory);
        window.statusBar()->showMessage(QObject::tr("%1 processes | Memory %2% | Updated every 2 seconds")
                .arg(model.rows.size())
                .arg(memory.dwMemoryLoad));
    };
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, refresh);
    timer.start(2000);
    refresh();
    window.show();
    if (app.arguments().contains("--ui-smoke-test"))
        QTimer::singleShot(500, &window, [&] {
            search->setText("no_such_process_for_filter_probe_7c12");
            const bool filtered = proxy->rowCount() == 0;
            search->clear();
            const bool ok = model.rowCount() > 0 && proxy->rowCount() == model.rowCount() && filtered
                && view->model()->columnCount() == 4 && window.findChild<QWidget*>("PlasmaBreezeTitleBar")
                && toolbar->actions().contains(end) && !window.statusBar()->currentMessage().isEmpty();
            window.grab().save("process-monitor-preview.png");
            QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                                << ": process table, filter/toolbar, status and Breeze main window\n";
            app.exit(ok ? 0 : 1);
        });
    return app.exec();
}

#include "process-monitor.moc"
