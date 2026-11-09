// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
class QVBoxLayout;
void addDirectSystemControls(QVBoxLayout* layout, int category);

#include <QByteArray>
#include <QString>
#include <functional>
class QLabel;
class QPushButton;
void runSettingsScript(
    QLabel*, QPushButton*, const QString&, std::function<void(QByteArray)> done = {}, bool updating = false);
void addUpdateControls(QVBoxLayout*, QLabel*);
