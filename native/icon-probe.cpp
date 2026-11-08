// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QTextStream>
#include <QIcon>
#include <QDir>
#include <KIconTheme>
#include <KIconLoader>
int main(int argc, char **argv) {
    if (argc < 2 || QByteArray(argv[1]) != "--no-init") KIconTheme::initTheme();
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    out << "Qt theme: " << QIcon::themeName() << " fallback: " << QIcon::fallbackThemeName() << " KDE: " << KIconTheme::current() << '\n';
    out << "Embedded themes: " << QDir(QStringLiteral(":/icons")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).join(',') << '\n';
    bool ok = true;
    for (const auto &name : {"folder", "application-zip", "application-x-ms-dos-executable", "unknown"}) {
        out << "Qt icon " << name << " null=" << QIcon::fromTheme(QString::fromLatin1(name)).pixmap(48, 48).isNull() << '\n';
        QString path;
        const auto image = KIconLoader::global()->loadIcon(QString::fromLatin1(name), KIconLoader::Desktop, 48, 0, {}, &path, true);
        out << name << ": " << path << " null=" << image.isNull() << '\n';
        ok &= !image.isNull();
    }
    return ok ? 0 : 1;
}
