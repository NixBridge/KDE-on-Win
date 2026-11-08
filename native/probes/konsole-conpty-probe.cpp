// SPDX-License-Identifier: GPL-3.0-or-later
#include <vector>
#include "conptyprocess.h"
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    ConPtyProcess process;
    const QString shell=QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot")+"/System32/cmd.exe");
    if(!process.isAvailable() || !process.startProcess(shell,{"/Q"},QDir::tempPath(),QProcessEnvironment::systemEnvironment().toStringList(),80,24)) {
        QTextStream(stdout)<<"FAIL: "<<process.lastError()<<'\n';return 1;
    }
    const bool resized=process.resize(100,30);
    QByteArray output;
    QElapsedTimer timer;timer.start();
    bool sent=false;
    while(timer.elapsed()<10000 && !output.contains("388")) {
        app.processEvents();const auto chunk=process.readAll();output+=chunk;
        // cmd's line editor queries cursor position from a real terminal.
        if(chunk.contains("\x1b[6n"))process.write("\x1b[1;1R",6);
        if(!sent && output.contains('>')) {process.write("set /a 137+251\r",15);sent=true;}
        QThread::msleep(20);
    }
    const bool ok=resized && output.contains("388");
    process.write("exit\r",5);
    timer.restart();while(timer.elapsed()<1000){app.processEvents();QThread::msleep(10);}
    process.kill();
    if(!ok)QTextStream(stdout)<<"resize="<<resized<<" commandSent="<<sent<<" output="<<output.toHex().left(2000)<<'\n';
    QTextStream(stdout)<<(ok?"PASS":"FAIL")<<": Konsole ConPTY interactive arithmetic output and terminal resize\n";
    return ok?0:1;
}
