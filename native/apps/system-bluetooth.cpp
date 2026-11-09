// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-controls.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <QMessageBox>
#include <windows.h>
#include <bluetoothapis.h>
#include <memory>
void addBluetoothDevices(QVBoxLayout* layout, QComboBox* radios, std::shared_ptr<QList<HANDLE>> handles, QLabel* status)
{
    auto* parent = layout->parentWidget();
    auto* devices = new QComboBox(parent);
    layout->addWidget(new QLabel(QObject::tr("Classic Bluetooth devices"), parent));
    layout->addWidget(devices);
    auto* scan = new QPushButton(QObject::tr("Search Bluetooth devices"), parent);
    layout->addWidget(scan);
    auto* pair = new QPushButton(QObject::tr("Pair selected device…"), parent);
    layout->addWidget(pair);
    auto* remove = new QPushButton(QObject::tr("Remove selected pairing…"), parent);
    layout->addWidget(remove);
    auto generation = std::make_shared<quint64>(0);
    QObject::connect(radios, &QComboBox::currentIndexChanged, parent, [=] {
        ++*generation;
        devices->clear();
    });
    QObject::connect(scan, &QPushButton::clicked, parent, [=] {
        const int index = radios->currentIndex();
        if (index < 0 || index >= handles->size()) {
            status->setText(QObject::tr("No Bluetooth radio found."));
            return;
        }
        HANDLE radio = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), handles->at(index), GetCurrentProcess(), &radio, 0, FALSE,
                DUPLICATE_SAME_ACCESS)) {
            status->setText(QObject::tr("Cannot access the Bluetooth radio."));
            return;
        }
        const quint64 revision = *generation;
        scan->setEnabled(false);
        status->setText(QObject::tr("Searching Bluetooth devices…"));
        struct Result {
            QList<BLUETOOTH_DEVICE_INFO> devices;
            DWORD error = 0;
        };
        auto result = std::make_shared<Result>();
        auto* thread = QThread::create([radio, result] {
            BLUETOOTH_DEVICE_SEARCH_PARAMS query {};
            query.dwSize = sizeof(query);
            query.fReturnAuthenticated = query.fReturnRemembered = query.fReturnUnknown = query.fReturnConnected = TRUE;
            query.fIssueInquiry = TRUE;
            query.cTimeoutMultiplier = 2;
            query.hRadio = radio;
            BLUETOOTH_DEVICE_INFO info {};
            info.dwSize = sizeof(info);
            auto find = BluetoothFindFirstDevice(&query, &info);
            if (find) {
                do {
                    result->devices.append(info);
                    info.dwSize = sizeof(info);
                } while (BluetoothFindNextDevice(find, &info));
                BluetoothFindDeviceClose(find);
            } else
                result->error = GetLastError();
            CloseHandle(radio);
        });
        QObject::connect(thread, &QThread::finished, parent, [=] {
            scan->setEnabled(true);
            if (revision != *generation)
                return;
            devices->clear();
            for (const auto& info : result->devices) {
                QString name = QString::fromWCharArray(info.szName);
                if (name.isEmpty())
                    name = QStringLiteral("%1").arg(info.Address.ullLong, 12, 16, QChar('0'));
                devices->addItem(name + (info.fAuthenticated ? QObject::tr(" (paired)") : QString()),
                    QByteArray(reinterpret_cast<const char*>(&info), sizeof(info)));
            }
            status->setText(QObject::tr("%1 Bluetooth devices found.").arg(devices->count()));
        });
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    });
    QObject::connect(pair, &QPushButton::clicked, parent, [=] {
        int index = radios->currentIndex();
        const auto bytes = devices->currentData().toByteArray();
        if (index < 0 || index >= handles->size() || bytes.size() != sizeof(BLUETOOTH_DEVICE_INFO))
            return;
        BLUETOOTH_DEVICE_INFO info;
        memcpy(&info, bytes.constData(), sizeof(info));
        DWORD error = BluetoothAuthenticateDeviceEx(reinterpret_cast<HWND>(parent->window()->winId()),
            handles->at(index), &info, nullptr, MITMProtectionRequiredBonding);
        status->setText(error ? QObject::tr("Bluetooth pairing failed or cancelled (%1).").arg(error)
                              : QObject::tr("Bluetooth device paired."));
    });
    QObject::connect(remove, &QPushButton::clicked, parent, [=] {
        const auto bytes = devices->currentData().toByteArray();
        if (bytes.size() != sizeof(BLUETOOTH_DEVICE_INFO))
            return;
        BLUETOOTH_DEVICE_INFO info;
        memcpy(&info, bytes.constData(), sizeof(info));
        if (QMessageBox::question(parent, QObject::tr("Remove Bluetooth device"),
                QObject::tr("Remove pairing with %1?").arg(QString::fromWCharArray(info.szName)))
            != QMessageBox::Yes)
            return;
        const DWORD error = BluetoothRemoveDevice(&info.Address);
        status->setText(error ? QObject::tr("Could not remove pairing (%1).").arg(error)
                              : QObject::tr("Pairing removed. Refresh the device list."));
    });
}
