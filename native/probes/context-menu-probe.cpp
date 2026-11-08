// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QElapsedTimer>
#include <QThread>
#include <QMenu>
#include <QTextStream>
#include <QDir>
#include <QUrl>
int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import org.kde.private.desktopcontainment.folder\nFolderModel {}", QUrl());
    QElapsedTimer timer; timer.start();
    while (component.isLoading() && timer.elapsed() < 10000) { app.processEvents(); QThread::msleep(10); }
    QObject *model = component.create();
    if (!model) return 1;
    model->setProperty("url", QUrl::fromLocalFile(QDir::tempPath()).toString());
    timer.restart();
    while (timer.elapsed() < 1000) { app.processEvents(); QThread::msleep(10); }
    const bool invoked = QMetaObject::invokeMethod(model, "openContextMenu", Q_ARG(QQuickItem *, nullptr), Q_ARG(Qt::KeyboardModifiers, Qt::NoModifier));
    timer.restart();
    while (timer.elapsed() < 300) { app.processEvents(); QThread::msleep(10); }
    bool ok = false;
    for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *menu = qobject_cast<QMenu *>(widget); menu && menu->isVisible()) {
            const auto image = menu->grab().toImage();
            ok = invoked && !menu->testAttribute(Qt::WA_TranslucentBackground)
                && menu->windowFlags().testFlag(Qt::WindowStaysOnTopHint) && !image.isNull();
            image.save(QStringLiteral("context-menu-preview.png"));
            QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": context menu visible, opaque, topmost; " << image.width() << 'x' << image.height() << '\n';
            menu->hide();
        }
    }
    delete model;
    return ok ? 0 : 1;
}
