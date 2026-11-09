// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTextStream>
#include <QMetaProperty>
#include "volumecontroller.h"
#include <windows.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized))
        return 2;
    bool ok = true;
    {
        VolumeController controller; // Read-only device queries; no setters.
        int updates = 0;
        QObject::connect(&controller, &VolumeController::iconNameChanged, &app, [&] { ++updates; });
        QMetaObject::invokeMethod(&controller, "mutedChanged", Qt::DirectConnection);
        QMetaObject::invokeMethod(&controller, "volumeChanged", Qt::DirectConnection);
        const int index = controller.metaObject()->indexOfProperty("iconName");
        ok = updates == 2 && index >= 0
            && controller.metaObject()->property(index).notifySignal().name() == "iconNameChanged";
    }
    // A failed STA initialization must not uninitialize this thread's MTA.
    const HRESULT different = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ok &= different == RPC_E_CHANGED_MODE;
    if (SUCCEEDED(different))
        CoUninitialize();
    CoUninitialize();
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": failed COM apartment initialization retains caller state; mute and volume both notify "
                           "icon binding; no audio changes\n";
    return ok ? 0 : 1;
}
