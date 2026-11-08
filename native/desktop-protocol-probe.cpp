// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <KProtocolInfo>
#include <KIO/ListJob>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("desktop"))) {
        out << "FAIL: desktop protocol not registered\n";
        return 1;
    }
    out << "PASS: desktop protocol registered\n";
    auto *job = KIO::listDir(QUrl(QStringLiteral("desktop:/")), KIO::HideProgressInfo);
    int count = 0;
    QObject::connect(job, &KIO::ListJob::entries, &app, [&](KIO::Job *, const KIO::UDSEntryList &entries) {
        count += entries.size();
    });
    QObject::connect(job, &KJob::result, &app, [&](KJob *result) {
        if (result->error()) {
            out << "FAIL: " << result->errorString() << '\n';
            app.exit(1);
        } else {
            out << "PASS: desktop:/ listed, " << count << " entries\n";
            app.exit(0);
        }
    });
    QTimer::singleShot(15000, &app, [&] {
        out << "FAIL: desktop worker timed out\n";
        app.exit(1);
    });
    return app.exec();
}
