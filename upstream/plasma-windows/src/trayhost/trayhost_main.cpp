/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>

#include "windowstrayhost.h"
#include "windowsinstancelock.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        const auto fixtureName = QStringLiteral("Local\\KDETrayFixture-%1").arg(GetCurrentProcessId()).toStdWString();
        WindowsInstanceLock first(fixtureName.c_str());
        WindowsInstanceLock second(fixtureName.c_str());
        const bool ok = first.valid() && !first.duplicate() && second.valid() && second.duplicate()
            && WindowsTrayHost::selfTest();
        QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": bounded tray payloads, unknown updates, GUID version, state mask and idempotent stop; no tray receiver or broadcast\n";
        return ok ? 0 : 1;
    }
    WindowsInstanceLock instance(L"Local\\NixBridge.KDE.TrayHost");
    if (!instance.valid()) return 2;
    if (instance.duplicate()) return 0;
    WindowsTrayHost host;
    if (!host.start()) {
        return 1;
    }

    // Periodic dead-icon cleanup only - the old 10s full-table dump was
    // debug noise in the log.
    QTimer cleanupTimer;
    cleanupTimer.setInterval(10000);
    QObject::connect(&cleanupTimer, &QTimer::timeout, &host, &WindowsTrayHost::cleanupDeadIcons);
    cleanupTimer.start();

    return app.exec();
}
