// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QDir>
#include "session/Session.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    bool ok = true;
    int finished = 0;
    int heartbeats = 0;
    qint64 worst = 0;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] { ++heartbeats; });
    timer.start(5);
    const QString shell = QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot") + "/System32/cmd.exe");
    for (int i = 0; i < 10 && ok; ++i) {
        Konsole::Session session;
        session.setProgram(shell);
        session.setArguments({ shell, "/Q" });
        session.setInitialWorkingDirectory(QDir::tempPath());
        QObject::connect(&session, &Konsole::Session::finished, &app, [&](Konsole::Session*) { ++finished; });
        session.setKeyBindings(QStringLiteral("default"));
        session.run();
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < 60) {
            app.processEvents();
            QThread::msleep(2);
        }
        ok &= session.isRunning();
        const int previous = finished;
        elapsed.restart();
        session.close();
        worst = qMax(worst, elapsed.elapsed());
        elapsed.restart();
        while (elapsed.elapsed() < 60) {
            app.processEvents();
            QThread::msleep(2);
        }
        ok &= !session.isRunning() && finished == previous + 1;
    }
    ok &= finished == 10 && heartbeats > 10 && worst < 500;
    QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": real Konsole Session close/recreate, finished=" << finished
                        << " heartbeat=" << heartbeats << " worst ms=" << worst << '\n';
    return ok ? 0 : 1;
}
