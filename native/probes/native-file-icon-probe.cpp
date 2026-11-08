// SPDX-License-Identifier: GPL-3.0-or-later
#include "startmenuimageprovider.h"
#include <QGuiApplication>
#include <QTextStream>
#include <QUrl>
#include <objbase.h>
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    StartMenuImageProvider provider;
    QTextStream out(stdout);
    bool ok = true;
    const auto args = app.arguments();
    const bool breeze = args.value(1) == QStringLiteral("--breeze");
    for (int i = breeze ? 2 : 1; i < args.size(); ++i) {
        QSize size;
        const auto image = provider.requestImage(QString::fromLatin1(QUrl::toPercentEncoding((breeze ? QStringLiteral("breeze:") : QString()) + QUrl::fromLocalFile(args[i]).toString())), &size, QSize(48,48));
        int pixels = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) pixels += qAlpha(image.pixel(x,y)) > 0;
        out << (pixels ? "PASS" : "FAIL") << ": native icon " << i << " visible pixels=" << pixels << '\n';
        if (pixels) image.save(QStringLiteral("native-file-icon-%1.png").arg(i));
        ok &= pixels > 0;
    }
    CoUninitialize();
    return ok ? 0 : 1;
}
