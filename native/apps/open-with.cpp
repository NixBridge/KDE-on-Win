// SPDX-License-Identifier: GPL-3.0-or-later
#include "localization.h"
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QUrl>
#include <QTextStream>
#include <QDesktopServices>
#include <QTemporaryDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <KIconTheme>
#include <KIconLoader>
#include <shlobj.h>
#include <wrl/client.h>
#include <vector>
using Microsoft::WRL::ComPtr;

struct Handler {
    QString id, name;
    ComPtr<IAssocHandler> native;
};
static std::vector<Handler> handlersFor(const QString& key, bool protocol)
{
    std::vector<Handler> result;
    ComPtr<IEnumAssocHandlers> enumeration;
    const HRESULT hr = protocol
        ? SHAssocEnumHandlersForProtocolByApplication(
              reinterpret_cast<LPCWSTR>(key.utf16()), IID_PPV_ARGS(&enumeration))
        : SHAssocEnumHandlers(reinterpret_cast<LPCWSTR>(key.utf16()), ASSOC_FILTER_NONE, &enumeration);
    if (FAILED(hr))
        return result;
    ComPtr<IAssocHandler> handler;
    ULONG count = 0;
    while (enumeration->Next(1, &handler, &count) == S_OK && count) {
        PWSTR id = nullptr, name = nullptr;
        handler->GetName(&id);
        handler->GetUIName(&name);
        Handler item { QString::fromWCharArray(id ? id : L""), QString::fromWCharArray(name ? name : L""), handler };
        CoTaskMemFree(id);
        CoTaskMemFree(name);
        const auto lower = item.id.toLower();
        if (!item.id.isEmpty() && !lower.contains("nixbridge.kdeopenwith") && !lower.contains("plasma-open-with"))
            result.push_back(std::move(item));
        handler.Reset();
    }
    return result;
}
static bool invokeHandler(IAssocHandler* handler, const QString& target)
{
    PIDLIST_ABSOLUTE id = nullptr;
    if (FAILED(SHParseDisplayName(reinterpret_cast<LPCWSTR>(target.utf16()), nullptr, &id, 0, nullptr)))
        return false;
    ComPtr<IShellFolder> parent;
    PCUITEMID_CHILD child = nullptr;
    ComPtr<IDataObject> data;
    HRESULT hr = SHBindToParent(id, IID_PPV_ARGS(&parent), &child);
    if (SUCCEEDED(hr))
        hr = parent->GetUIObjectOf(
            nullptr, 1, &child, IID_IDataObject, nullptr, reinterpret_cast<void**>(data.GetAddressOf()));
    if (SUCCEEDED(hr))
        hr = handler->Invoke(data.Get());
    CoTaskMemFree(id);
    return SUCCEEDED(hr);
}
// Test-owned endpoint: real Shell data dispatch without opening an application.
class ProbeHandler final : public IAssocHandler {
public:
    QString expected;
    int calls = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != IID_IAssocHandler)
            return E_NOINTERFACE;
        *out = static_cast<IAssocHandler*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE GetName(LPWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetUIName(LPWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetIconLocation(LPWSTR*, int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE IsRecommended() override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE MakeDefault(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateInvoker(IDataObject*, IAssocHandlerInvoker**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Invoke(IDataObject* data) override
    {
        ++calls;
        FORMATETC format { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium {};
        if (!data || FAILED(data->GetData(&format, &medium)))
            return E_FAIL;
        wchar_t path[32768] {};
        const UINT count = DragQueryFileW(static_cast<HDROP>(medium.hGlobal), 0xffffffff, nullptr, 0);
        DragQueryFileW(static_cast<HDROP>(medium.hGlobal), 0, path, 32768);
        const bool ok = count == 1 && QDir::fromNativeSeparators(QString::fromWCharArray(path)) == expected;
        ReleaseStgMedium(&medium);
        return ok ? S_OK : E_FAIL;
    }

private:
    ULONG references = 1;
};
int main(int argc, char** argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    installProjectTranslations(&app);
    if (app.arguments().contains("--invoke-self-test")) {
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        QTemporaryDir directory;
        QFile file(directory.path() + QStringLiteral("/space & unicode-测试.txt"));
        bool ok = file.open(QIODevice::WriteOnly);
        file.close();
        ProbeHandler handler;
        handler.expected = file.fileName();
        ok &= invokeHandler(&handler, QDir::toNativeSeparators(file.fileName())) && handler.calls == 1;
        if (SUCCEEDED(apartment))
            CoUninitialize();
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": real Shell IDataObject delivered once to a test-owned handler with Unicode path "
                               "preserved; no program invoked\n";
        return ok ? 0 : 1;
    }
    const bool smoke = app.arguments().contains("--ui-smoke-test");
    if (smoke) {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
        app.setFont(QFont(defaultProjectFontFamily(), 10));
    }
    const bool chinese = projectLanguage().startsWith("zh");
    auto text = [chinese](const char* en, const char* zh) { return QString::fromUtf8(chinese ? zh : en); };
    if (app.arguments().contains("--configure-defaults"))
        return QDesktopServices::openUrl(
                   QUrl("ms-settings:defaultapps?registeredAppUser=KDE-on-Win%20Application%20Chooser"))
            ? 0
            : 1;
    QString target = smoke ? QStringLiteral("mailto:test@example.invalid") : app.arguments().value(1);
    QUrl url(target);
    if (url.isLocalFile())
        target = url.toLocalFile();
    const bool protocol = !url.isLocalFile() && !QFileInfo(target).exists() && url.isValid() && url.scheme().size() > 1;
    const QString key = protocol ? url.scheme().toLower() : "." + QFileInfo(target).suffix().toLower();
    if (target.isEmpty() || (!protocol && !QFileInfo(target).isFile()))
        return 2;
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    auto handlers = handlersFor(key, protocol);
    QMainWindow window;
    window.setWindowTitle(text("Open With", "选择应用打开"));
    window.setWindowIcon(
        KIconLoader::global()->loadIcon("preferences-desktop-filetype-association", KIconLoader::Desktop, 48));
    window.resize(560, 470);
    auto* body = new QWidget(&window);
    auto* layout = new QVBoxLayout(body);
    auto* heading = new QLabel(text("Choose an application", "选择应用程序"));
    auto font = heading->font();
    font.setPointSize(font.pointSize() + 3);
    heading->setFont(font);
    layout->addWidget(heading);
    auto* description = new QLabel(target.left(512));
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* search = new QLineEdit;
    search->setPlaceholderText(text("Search applications…", "搜索应用程序…"));
    layout->addWidget(search);
    auto* list = new QListWidget;
    list->setIconSize(QSize(32, 32));
    for (const auto& handler : handlers) {
        auto* item = new QListWidgetItem(
            KIconLoader::global()->loadIcon(
                protocol ? "internet-mail" : "application-x-executable", KIconLoader::Desktop, 32),
            handler.name.isEmpty() ? handler.id : handler.name, list);
        item->setData(Qt::UserRole, handler.id);
    }
    layout->addWidget(list, 1);
    if (list->count())
        list->setCurrentRow(0);
    else
        layout->addWidget(new QLabel(
            text("No registered applications. Choose a program below.", "没有已注册的应用，请在下方选择程序。")));
    QObject::connect(search, &QLineEdit::textChanged, list, [list](const QString& value) {
        int first = -1;
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setHidden(!list->item(row)->text().contains(value, Qt::CaseInsensitive));
            if (!list->item(row)->isHidden() && first < 0)
                first = row;
        }
        list->setCurrentRow(first);
    });
    auto* remember = new QCheckBox(text("Remember this choice in KDE", "在 KDE 中记住此选择"));
    layout->addWidget(remember);
    auto* status = new QLabel;
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* buttons = new QHBoxLayout;
    auto* browse = new QPushButton(text("Choose a program…", "选择其他程序…"));
    auto* cancel = new QPushButton(text("Cancel", "取消"));
    auto* open = new QPushButton(text("Open", "打开"));
    open->setEnabled(list->count() > 0);
    QObject::connect(list, &QListWidget::currentRowChanged, open, [open](int row) { open->setEnabled(row >= 0); });
    buttons->addWidget(browse);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(open);
    layout->addLayout(buttons);
    window.setCentralWidget(body);
    QSettings settings(appearanceSettingsPath(), QSettings::IniFormat);
    const QString preference = "openWith/" + key;
    auto invoke = [&](int row, bool automatic) {
        if (smoke || row < 0 || row >= int(handlers.size()))
            return false;
        if (!invokeHandler(handlers[row].native.Get(), target)) {
            if (automatic)
                settings.remove(preference);
            status->setText(text("Could not start the selected application.", "无法启动所选应用程序。"));
            return false;
        }
        if (remember->isChecked())
            settings.setValue(preference, handlers[row].id);
        window.close();
        return true;
    };
    QObject::connect(open, &QPushButton::clicked, &window, [&] { invoke(list->currentRow(), false); });
    QObject::connect(
        list, &QListWidget::itemDoubleClicked, &window, [&](QListWidgetItem*) { invoke(list->currentRow(), false); });
    QObject::connect(cancel, &QPushButton::clicked, &window, &QWidget::close);
    QObject::connect(browse, &QPushButton::clicked, &window, [&] {
        if (smoke)
            return;
        const QString program = QFileDialog::getOpenFileName(&window, text("Choose a program", "选择程序"), {},
            "Programs (*.exe)", nullptr, QFileDialog::DontUseNativeDialog);
        if (program.isEmpty()
            || QFileInfo(program).canonicalFilePath().compare(
                   QFileInfo(app.applicationFilePath()).canonicalFilePath(), Qt::CaseInsensitive)
                == 0)
            return;
        if (QProcess::startDetached(program, { target }))
            window.close();
        else
            status->setText(text("Could not start the selected application.", "无法启动所选应用程序。"));
    });
    if (smoke) {
        window.ensurePolished();
        app.processEvents();
        const bool ok = list && search && browse && open && remember
            && window.findChild<QWidget*>("PlasmaBreezeTitleBar")
            && QFontMetrics(window.font()).inFontUcs4(chinese ? 0x9009 : 0x41)
            && QFontMetrics(window.font()).inFontUcs4(0x41)
            && (!app.arguments().contains("--expect-chinese")
                || window.windowTitle() == QStringLiteral("选择应用打开"));
        window.grab().save("open-with-preview.png");
        handlers.clear();
        if (SUCCEEDED(apartment))
            CoUninitialize();
        QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                            << ": KDE application chooser UI and native handler enumeration; no application invoked\n";
        return ok ? 0 : 1;
    }
    const auto saved = settings.value(preference).toString();
    bool launched = false;
    for (int row = 0; row < int(handlers.size()) && !saved.isEmpty(); ++row)
        if (handlers[row].id == saved) {
            launched = invoke(row, true);
            break;
        }
    int result = 0;
    if (!launched) {
        window.show();
        result = app.exec();
    }
    handlers.clear();
    if (SUCCEEDED(apartment))
        CoUninitialize();
    return result;
}
