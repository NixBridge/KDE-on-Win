// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <QAbstractItemModel>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    QTextStream out(stdout);
    out << "QML search paths:\n" << engine.importPathList().join('\n') << '\n';
    QQmlComponent component(&engine);
    if(app.arguments().contains(QStringLiteral("--check-startmenu-model"))) {
        component.setData("import QtQml\nimport org.kde.plasma.windowsmenu\nStartMenuModel {}",QUrl());
        std::unique_ptr<QObject> object(component.create());
        auto *model=qobject_cast<QAbstractItemModel*>(object.get());
        out<<"Model="<<(model?"yes":"no")<<" columns="<<(object?object->property("columns").toString():QString())<<" iconPixels="<<(object?object->property("iconPixels").toString():QString())<<'\n';
        bool ok=model && object->property("columns").toInt()>=2 && object->property("iconPixels").toInt()>=24;
        if(ok){object->setProperty("directory",QStringLiteral("all"));const int count=model->rowCount();object->setProperty("searchText",QStringLiteral("__missing_startmenu_probe_9cc1"));out<<"Before="<<count<<" filtered="<<model->rowCount()<<'\n';ok &= count>0 && model->rowCount()==0;object->setProperty("searchText",QString());out<<"Restored="<<model->rowCount()<<'\n';ok &= model->rowCount()==count;}
        for(const auto &error:component.errors())out<<error.toString()<<'\n';
        out<<(ok?"PASS":"FAIL")<<": installed Windows menu QML plugin properties and live search\n";return ok?0:1;
    }
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
import QtQuick.Layouts as Layouts
import QtQuick.Effects as Effects
import org.kde.kirigami
import org.kde.plasma.activityswitcher
import org.kde.kwindowsystem
import org.kde.plasma.core
import org.kde.quickcharts as Charts
import org.kde.quickcharts.controls
QtObject {
    property QtObject chart: Charts.BarChart {}
    property QtObject layouts: Layouts.RowLayout {}
    property QtObject effects: Effects.MultiEffect {}
}
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
