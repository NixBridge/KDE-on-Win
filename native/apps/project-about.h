// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include <QMenuBar>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
inline void addProjectAbout(QMainWindow* window)
{
    auto* help = window->menuBar()->addMenu(QObject::tr("&Help"));
    help->addAction(QObject::tr("Project repository"), window,
        [] { QDesktopServices::openUrl(QUrl("https://github.com/NixBridge/KDE-on-Win")); });
    help->addAction(QObject::tr("About KDE-on-Win"), window, [window] {
        QMessageBox::about(window, QObject::tr("About KDE-on-Win"),
            QObject::tr("KDE-on-Win project-owned application. Licensed under GPL-3.0-or-later.")
                + "<br><a "
                  "href=\"https://github.com/NixBridge/KDE-on-Win\">https://github.com/NixBridge/KDE-on-Win</a>");
    });
}
