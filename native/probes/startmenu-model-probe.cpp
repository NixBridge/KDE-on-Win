// SPDX-License-Identifier: GPL-3.0-or-later
#include "startmenumodel.h"
#include "startmenuimageprovider.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <KIconTheme>
#include <shlobj.h>
static bool shortcut(const QString& path, const QString& target, const QString& arguments)
{
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))))
        return false;
    link->SetPath(reinterpret_cast<LPCWSTR>(target.utf16()));
    link->SetArguments(reinterpret_cast<LPCWSTR>(arguments.utf16()));
    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
        ok = SUCCEEDED(file->Save(reinterpret_cast<LPCWSTR>(path.utf16()), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}
int main(int argc, char** argv)
{
    KIconTheme::initTheme();
    QGuiApplication app(argc, argv);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    QTemporaryDir temp;
    const QString a = temp.path() + QStringLiteral("/user"), b = temp.path() + QStringLiteral("/common");
    QDir().mkpath(a + "/Tools");
    QDir().mkpath(b + "/Tools");
    bool ok = shortcut(a + "/Tools/Same.lnk", app.applicationFilePath(), "CaseSensitive")
        && shortcut(b + "/Tools/Same.lnk", app.applicationFilePath(), "CaseSensitive")
        && shortcut(b + "/Tools/Other.lnk", app.applicationFilePath(), "Other");
    QFile document(a + "/Tools/readme.txt");
    document.open(QIODevice::WriteOnly);
    document.write("fixture");
    document.close();
    StartMenuModel model;
    const QString merged = QStringLiteral("merged:")
        + QString::fromUtf8(QJsonDocument(QJsonArray { a, b }).toJson(QJsonDocument::Compact));
    model.setDirectory(merged);
    ok &= model.rowCount() == 1 && model.data(model.index(0), StartMenuModel::IsDirRole).toBool();
    model.activate(0);
    ok &= model.rowCount() == 3;
    model.setSearchText(QStringLiteral("same"));
    ok &= model.rowCount() == 1;
    model.setSearchText(QString());
    model.goParent();
    ok &= model.directory() == merged && model.rowCount() == 1;
    StartMenuImageProvider images;
    QSize size;
    ok &= !images.requestImage(QStringLiteral("breeze:theme:folder"), &size, QSize(48, 48)).isNull();
    const QImage native = images.requestImage(a + QStringLiteral("/Tools/Same.lnk"), &size, QSize(48, 48));
    const QImage themed
        = images.requestImage(QStringLiteral("breeze:") + a + QStringLiteral("/Tools/Same.lnk"), &size, QSize(48, 48));
    ok &= !native.isNull() && native == themed;
    model.setDirectory(QStringLiteral("all"));
    QStringList before;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString link = model.data(model.index(row), StartMenuModel::LinkPathRole).toString();
        if (link.startsWith(QLatin1String("apps:")))
            before.append(link);
    }
    model.setDirectory(merged);
    model.goParent();
    QStringList after;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString link = model.data(model.index(row), StartMenuModel::LinkPathRole).toString();
        if (link.startsWith(QLatin1String("apps:")))
            after.append(link);
    }
    before.sort();
    after.sort();
    ok &= before == after;
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": merged folders, duplicate shortcuts, nested files, search/back, rendered "
                           "folder/third-party icons, AppsFolder cache round trip\n";
    CoUninitialize();
    return ok ? 0 : 1;
}
