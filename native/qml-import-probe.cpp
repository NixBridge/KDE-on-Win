// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    QTextStream out(stdout);
    out << "QML search paths:\n" << engine.importPathList().join('\n') << '\n';
    QQmlComponent component(&engine);
    const int checkIndex = app.arguments().indexOf(QStringLiteral("--check-component"));
    if (checkIndex >= 0 && checkIndex + 1 < app.arguments().size()) {
        component.loadUrl(QUrl::fromLocalFile(app.arguments().at(checkIndex + 1)), QQmlComponent::PreferSynchronous);
        for (const auto &error : component.errors()) {
            out << error.toString() << '\n';
        }
        out << (component.isReady() ? "PASS: QML component compiled\n" : "FAIL: QML component compilation\n");
        return component.isReady() ? 0 : 1;
    }
    component.setData(R"(
import QtQml
import QtQuick
import org.kde.kirigami
import org.kde.plasma.activityswitcher
import org.kde.kwindowsystem
import org.kde.plasma.core
import org.kde.quickcharts as Charts
import org.kde.quickcharts.controls
QtObject { property QtObject chart: Charts.BarChart {} }
)", QUrl("file:///qml-import-probe.qml"));
    QElapsedTimer timer;
    timer.start();
    while (component.isLoading() && timer.elapsed() < 10000) {
        app.processEvents();
        QThread::msleep(10);
    }
    QObject *object = component.isReady() ? component.create() : nullptr;
    for (const auto &error : component.errors()) {
        out << error.toString() << '\n';
    }
    out << (object ? "PASS: desktop QML imports loaded\n" : "FAIL: desktop QML imports unavailable\n");
    const bool ok = object;
    delete object;
    return ok ? 0 : 1;
}
