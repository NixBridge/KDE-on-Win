// SPDX-License-Identifier: GPL-3.0-or-later
#include <vector>
#include "conptyprocess.h"
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ConPtyProcess process;
    const QString shell = QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot") + "/System32/cmd.exe");
    if (!process.isAvailable()
        || !process.startProcess(
            shell, { "/Q" }, QDir::tempPath(), QProcessEnvironment::systemEnvironment().toStringList(), 80, 24)) {
        QTextStream(stdout) << "FAIL: " << process.lastError() << '\n';
        return 1;
    }
    const bool resized = process.resize(100, 30);
    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    bool sent = false;
    while (timer.elapsed() < 10000 && !output.contains("388")) {
        app.processEvents();
        const auto chunk = process.readAll();
        output += chunk;
        // cmd's line editor queries cursor position from a real terminal.
        if (chunk.contains("\x1b[6n"))
            process.write("\x1b[1;1R", 6);
        if (!sent && output.contains('>')) {
            process.write("set /a 137+251\r", 15);
            sent = true;
        }
        QThread::msleep(20);
    }
    bool ok = resized && output.contains("388");
    process.write("exit\r", 5);
    timer.restart();
    while (timer.elapsed() < 1000) {
        app.processEvents();
        QThread::msleep(10);
    }
    process.kill();
    // Close live shells repeatedly, including an idle reader. A close must
    // return promptly and the same backend must support the next session.
    int exits = 0;
    QObject::connect(&process, &ConPtyProcess::exited, &app, [&] { ++exits; });
    qint64 worstClose = 0;
    for (int i = 0; i < 20 && ok; ++i) {
        ok &= process.startProcess(
            shell, { "/Q" }, QDir::tempPath(), QProcessEnvironment::systemEnvironment().toStringList(), 80, 24);
        timer.restart();
        while (timer.elapsed() < 40) {
            app.processEvents();
            QThread::msleep(2);
        }
        timer.restart();
        ok &= process.kill();
        worstClose = qMax(worstClose, timer.elapsed());
        app.processEvents();
    }
    ok &= worstClose < 500 && exits == 0;
    QTextStream(stdout) << "live closes=20 worst ms=" << worstClose << " spurious exits=" << exits << '\n';
    // Profile-only variables must inherit the normal Windows environment.
    ok &= process.startProcess(shell, { "/Q", "/C", "if defined PATH (exit 7) else (exit 9)" },
        QDir::tempPath(), { "TERM=xterm-256color" }, 80, 24);
    timer.restart();
    while (timer.elapsed() < 2000 && exits == 0) {
        app.processEvents();
        QThread::msleep(5);
    }
    ok &= exits == 1 && process.exitCode() == 7;
    process.kill();
    // Queue an old exit without dispatching it, then reuse the backend.
    ok &= process.startProcess(shell, { "/Q", "/C", "exit 0" }, QDir::tempPath(), {}, 80, 24);
    QThread::msleep(150);
    process.kill();
    ok &= process.startProcess(shell, { "/Q" }, QDir::tempPath(), {}, 80, 24);
    timer.restart();
    while (timer.elapsed() < 100) {
        app.processEvents();
        QThread::msleep(2);
    }
    ok &= exits == 1;
    process.kill();
    QTextStream(stdout) << "natural exits=" << exits << "; profile inheritance and stale-callback suppression checked\n";
    if (!ok)
        QTextStream(stdout) << "resize=" << resized << " commandSent=" << sent
                            << " output=" << output.toHex().left(2000) << '\n';
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": Konsole ConPTY interactive arithmetic output and terminal resize\n";
    return ok ? 0 : 1;
}
