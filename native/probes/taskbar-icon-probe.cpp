// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QTextStream>
#include <QSet>
#include <KIconTheme>
#include "tasktools.h"
#include "windowsdesktopapps.h"

int main(int argc, char **argv)
{
    KIconTheme::initTheme();
    QGuiApplication application(argc, argv);
    QSet<QByteArray> images;
    bool ok = true;
    for (const auto &entry : TaskManager::windowsDesktopApps) {
        const auto data = TaskManager::appDataFromUrl(QUrl(QStringLiteral("applications:") + QLatin1String(entry.desktop)));
        const QImage image = data.icon.pixmap(48, 48).toImage().convertToFormat(QImage::Format_ARGB32);
        const auto *mapped = TaskManager::windowsDesktopApp(QStringLiteral("D:/KDE/CraftRoot/bin/") + QLatin1String(entry.exe));
        const bool valid = !image.isNull() && mapped == &entry;
        if (!image.isNull()) images.insert(QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Sha256));
        QTextStream(stdout) << (valid ? "PASS " : "FAIL ") << entry.desktop << " -> " << entry.icon << '\n';
        ok &= valid;
    }
    ok &= images.size() == std::size(TaskManager::windowsDesktopApps);
    QTextStream(stdout) << "Distinct rendered icons: " << images.size() << '\n';
    return ok ? 0 : 1;
}
