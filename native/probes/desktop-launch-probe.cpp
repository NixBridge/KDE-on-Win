// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QTextStream>
#include <QUrl>
#include <QElapsedTimer>
#include <QThread>
#include <objbase.h>
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    app.setQuitOnLastWindowClosed(false);
    const auto args = app.arguments();
    if (args.value(1) == QStringLiteral("--child")) {
        QFile out(args.value(2));
        if (!out.open(QIODevice::WriteOnly)) return 2;
        out.write(QJsonDocument(QJsonObject{{"arguments", QJsonArray::fromStringList(args.mid(3))}, {"cwd", QDir::currentPath()}}).toJson());
        return 0;
    }
    if (args.size() < 3) return 2;
    QQmlEngine engine;
    QQmlComponent component(&engine);
    const auto quoted = QJsonDocument(QJsonArray{QUrl::fromLocalFile(args[1]).toString()}).toJson(QJsonDocument::Compact);
    component.setData("import org.kde.private.desktopcontainment.folder\nFolderModel { url: " + quoted.mid(1, quoted.size()-2) + " }", QUrl());
    QElapsedTimer loadTimer;
    loadTimer.start();
    while (component.isLoading() && loadTimer.elapsed() < 10000) {
        app.processEvents();
        QThread::msleep(10);
    }
    QObject *object = component.create();
    QTextStream(stdout) << "Component status=" << component.status() << " object=" << (object ? object->metaObject()->className() : "null") << Qt::endl;
    if (!object) {
        for (const auto &error : component.errors()) QTextStream(stdout) << error.toString() << '\n';
        return 1;
    }
    bool launched = false;
    bool verified = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] {
        int count = 0;
        QMetaObject::invokeMethod(object, "rowCount", Q_RETURN_ARG(int, count));
        if (!launched && count > 0) {
            launched = QMetaObject::invokeMethod(object, "run", Q_ARG(int, 0));
        }
        if (QFile::exists(args[2])) {
            verified = true;
            QTextStream(stdout) << "PASS: FolderModel::run launched the Windows shortcut\n";
            app.exit(0);
        }
    });
    timer.start(100);
    QTimer::singleShot(15000, &app, [&] { QTextStream(stdout) << "FAIL: shortcut launch timed out\n"; app.exit(1); });
    QElapsedTimer verifyTimer;
    verifyTimer.start();
    while (!verified && verifyTimer.elapsed() < 15000) {
        app.processEvents();
        QThread::msleep(10);
    }
    if (!verified) QTextStream(stdout) << "FAIL: no child result, status=" << object->property("status").toInt() << " invoked=" << launched << Qt::endl;
    delete object;
    return verified ? 0 : 1;
}
