// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-controls.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDir>
#include <QHostAddress>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QDBusPendingCall>
#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <bluetoothapis.h>
#include <powrprof.h>
#include <lm.h>
#include <memory>
#include <functional>
#include <notificationmanager/settings.h>
#include <QDateTime>
#include <QRegularExpression>
void addBluetoothDevices(QVBoxLayout*, QComboBox*, std::shared_ptr<QList<HANDLE>>, QLabel*);
namespace {
QString errorText(DWORD code)
{
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    QString result = text ? QString::fromWCharArray(text).trimmed() : QStringLiteral("Windows error %1").arg(code);
    if (text)
        LocalFree(text);
    return result;
}
bool currentLocalUser(const QString& name)
{
    LPBYTE info = nullptr;
    if (NetUserGetInfo(nullptr, reinterpret_cast<LPCWSTR>(name.utf16()), 23, &info) != NERR_Success)
        return false;
    HANDLE token = nullptr;
    bool same = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        DWORD bytes = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
        QByteArray buffer(int(bytes), 0);
        if (bytes && GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes))
            same = EqualSid(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,
                reinterpret_cast<USER_INFO_23*>(info)->usri23_user_sid);
        CloseHandle(token);
    }
    NetApiBufferFree(info);
    return same;
}
void result(QWidget* parent, DWORD code)
{
    if (code)
        QMessageBox::warning(parent, QObject::tr("Could not apply settings"), errorText(code));
}
// Fixed scripts only. User data is validated as numeric IDs, IP addresses or
// known GUIDs before inclusion. Passwords never pass through command lines.
void script(QLabel* status, QPushButton* button, const QString& body, std::function<void(QByteArray)> done = {},
    bool updating = false)
{
    auto* p = new QProcess(status);
    button->setEnabled(false);
    if (updating)
        status->window()->setProperty("updatesRunning", true);
    status->setText(QObject::tr("Working…"));
    const QString code
        = "[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$ErrorActionPreference='Stop';try { " + body
        + " } catch { [Console]::Error.WriteLine($_.Exception.Message);exit 1 }";
    const QByteArray utf16(reinterpret_cast<const char*>(code.utf16()), code.size() * 2);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(p, &QProcess::finished, status, [=](int exit, QProcess::ExitStatus kind) {
        button->setEnabled(true);
        if (updating)
            status->window()->setProperty("updatesRunning", false);
        const auto output = p->readAllStandardOutput();
        status->setText(exit == 0 && kind == QProcess::NormalExit
                ? QObject::tr("Completed.")
                : QString::fromUtf8(p->readAllStandardError()).trimmed());
        if (exit == 0 && kind == QProcess::NormalExit && done)
            done(output);
        p->deleteLater();
    });
    QObject::connect(p, &QProcess::errorOccurred, status, [=](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            button->setEnabled(true);
            if (updating)
                status->window()->setProperty("updatesRunning", false);
            status->setText(p->errorString());
            p->deleteLater();
        }
    });
    QObject::connect(status, &QObject::destroyed, p, [p] {
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    });
    p->start(qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe",
        { "-NoLogo", "-NoProfile", "-NonInteractive", "-EncodedCommand", QString::fromLatin1(utf16.toBase64()) });
}
struct Endpoint {
    HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* volume = nullptr;
    explicit Endpoint(const QString& id = {})
    {
        if (SUCCEEDED(CoCreateInstance(
                __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator))))
            if (SUCCEEDED(id.isEmpty() ? enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)
                                       : enumerator->GetDevice(reinterpret_cast<LPCWSTR>(id.utf16()), &device)))
                device->Activate(
                    __uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&volume));
    }
    ~Endpoint()
    {
        if (volume)
            volume->Release();
        if (device)
            device->Release();
        if (enumerator)
            enumerator->Release();
        if (SUCCEEDED(apartment))
            CoUninitialize();
    }
};
}
void runSettingsScript(
    QLabel* status, QPushButton* button, const QString& body, std::function<void(QByteArray)> done, bool updating)
{
    script(status, button, body, done, updating);
}
void addDirectSystemControls(QVBoxLayout* layout, int category)
{
    auto* parent = layout->parentWidget();
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto* status = new QLabel(parent);
    status->setWordWrap(true);
    auto button = [&](const QString& text) {
        auto* b = new QPushButton(text, parent);
        layout->addWidget(b);
        return b;
    };
    if (category == 0) {
        auto* display = new QComboBox(parent);
        auto* modes = new QComboBox(parent);
        form->addRow(QObject::tr("Monitor"), display);
        form->addRow(QObject::tr("Resolution / refresh rate"), modes);
        auto* orientation = new QComboBox(parent);
        orientation->addItem(QObject::tr("Landscape"), DMDO_DEFAULT);
        orientation->addItem(QObject::tr("Portrait"), DMDO_90);
        orientation->addItem(QObject::tr("Landscape (flipped)"), DMDO_180);
        orientation->addItem(QObject::tr("Portrait (flipped)"), DMDO_270);
        form->addRow(QObject::tr("Orientation"), orientation);
        for (DWORD n = 0;; ++n) {
            DISPLAY_DEVICEW d {};
            d.cb = sizeof(d);
            if (!EnumDisplayDevicesW(nullptr, n, &d, 0))
                break;
            if ((d.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                && !(d.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
                display->addItem(QString::fromWCharArray(d.DeviceString), QString::fromWCharArray(d.DeviceName));
        }
        auto load = [=] {
            modes->clear();
            auto name = display->currentData().toString();
            DEVMODEW current {};
            current.dmSize = sizeof(current);
            EnumDisplaySettingsExW(reinterpret_cast<LPCWSTR>(name.utf16()), ENUM_CURRENT_SETTINGS, &current, 0);
            orientation->setCurrentIndex(qMax(0, orientation->findData(int(current.dmDisplayOrientation))));
            for (DWORD i = 0;; ++i) {
                DEVMODEW m {};
                m.dmSize = sizeof(m);
                if (!EnumDisplaySettingsExW(reinterpret_cast<LPCWSTR>(name.utf16()), i, &m, 0))
                    break;
                if (m.dmBitsPerPel != 32)
                    continue;
                QString title
                    = QStringLiteral("%1 × %2, %3 Hz").arg(m.dmPelsWidth).arg(m.dmPelsHeight).arg(m.dmDisplayFrequency);
                if (modes->findText(title) >= 0)
                    continue;
                modes->addItem(title, QByteArray(reinterpret_cast<char*>(&m), sizeof(m)));
                if (m.dmPelsWidth == current.dmPelsWidth && m.dmPelsHeight == current.dmPelsHeight
                    && m.dmDisplayFrequency == current.dmDisplayFrequency)
                    modes->setCurrentIndex(modes->count() - 1);
            }
        };
        QObject::connect(display, &QComboBox::currentIndexChanged, parent, load);
        load();
        auto* apply = button(QObject::tr("Test display mode…"));
        apply->setEnabled(modes->count() > 0);
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            const auto bytes = modes->currentData().toByteArray();
            if (bytes.size() != sizeof(DEVMODEW))
                return;
            DEVMODEW selected;
            memcpy(&selected, bytes.constData(), sizeof(selected));
            DEVMODEW old {};
            old.dmSize = sizeof(old);
            auto name = display->currentData().toString();
            auto device = reinterpret_cast<LPCWSTR>(name.utf16());
            if (!EnumDisplaySettingsExW(device, ENUM_CURRENT_SETTINGS, &old, 0))
                return;
            const DWORD rotation = orientation->currentData().toUInt();
            if ((selected.dmDisplayOrientation % 2) != (rotation % 2))
                std::swap(selected.dmPelsWidth, selected.dmPelsHeight);
            selected.dmDisplayOrientation = rotation;
            selected.dmFields |= DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;
            const LONG tested = ChangeDisplaySettingsExW(device, &selected, nullptr, CDS_TEST, nullptr);
            if (tested != DISP_CHANGE_SUCCESSFUL) {
                status->setText(QObject::tr("The driver rejected this mode (%1).").arg(tested));
                return;
            }
            const LONG changed = ChangeDisplaySettingsExW(device, &selected, nullptr, 0, nullptr);
            if (changed != DISP_CHANGE_SUCCESSFUL) {
                status->setText(QObject::tr("Display change failed (%1).").arg(changed));
                return;
            }
            QMessageBox confirm(QMessageBox::Question, QObject::tr("Keep display mode?"),
                QObject::tr("Reverting in 15 seconds unless you choose Yes."), QMessageBox::Yes | QMessageBox::No,
                parent);
            confirm.setDefaultButton(QMessageBox::No);
            QTimer::singleShot(15000, &confirm, &QMessageBox::reject);
            const bool keep = confirm.exec() == QMessageBox::Yes;
            const LONG saved = ChangeDisplaySettingsExW(
                device, keep ? &selected : &old, nullptr, keep ? CDS_UPDATEREGISTRY : 0, nullptr);
            if (saved != DISP_CHANGE_SUCCESSFUL)
                ChangeDisplaySettingsExW(device, &old, nullptr, 0, nullptr);
            status->setText(keep && saved == DISP_CHANGE_SUCCESSFUL ? QObject::tr("Display mode saved.")
                                                                    : QObject::tr("Previous display mode restored."));
        });
    } else if (category == 1) {
        auto* devices = new QComboBox(parent);
        form->addRow(QObject::tr("Device to adjust"), devices);
        auto* volume = new QSlider(Qt::Horizontal, parent);
        volume->setRange(0, 100);
        auto* mute = new QCheckBox(QObject::tr("Mute"), parent);
        form->addRow(QObject::tr("Device volume"), volume);
        form->addRow(mute);
        auto refresh = [=] {
            Endpoint e(devices->currentData().toString());
            float level = 0;
            BOOL muted = FALSE;
            const bool ok = e.volume && SUCCEEDED(e.volume->GetMasterVolumeLevelScalar(&level))
                && SUCCEEDED(e.volume->GetMute(&muted));
            volume->setEnabled(ok);
            mute->setEnabled(ok);
            if (ok) {
                volume->setValue(qRound(level * 100));
                mute->setChecked(muted);
            }
            status->setText(ok ? QObject::tr("Adjust volume and mute for the selected device. This does not change the default output device.")
                               : QObject::tr("No playback device available."));
        };
        auto* reload = button(QObject::tr("Refresh devices"));
        auto enumerate = [=] {
            const QString previous = devices->currentData().toString();
            devices->clear();
            devices->addItem(QObject::tr("Default device"), QString());
            Endpoint endpoint;
            IMMDeviceCollection* collection = nullptr;
            if (endpoint.enumerator
                && SUCCEEDED(endpoint.enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
                UINT count = 0;
                collection->GetCount(&count);
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice* device = nullptr;
                    if (FAILED(collection->Item(i, &device)))
                        continue;
                    LPWSTR id = nullptr;
                    device->GetId(&id);
                    QString label;
                    IPropertyStore* store = nullptr;
                    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
                        PROPVARIANT value;
                        PropVariantInit(&value);
                        if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
                            label = QString::fromWCharArray(value.pwszVal);
                        PropVariantClear(&value);
                        store->Release();
                    }
                    if (id) {
                        devices->addItem(
                            label.isEmpty() ? QString::fromWCharArray(id) : label, QString::fromWCharArray(id));
                        CoTaskMemFree(id);
                    }
                    device->Release();
                }
                collection->Release();
            }
            devices->setCurrentIndex(qMax(0, devices->findData(previous)));
            refresh();
        };
        QObject::connect(reload, &QPushButton::clicked, parent, enumerate);
        QObject::connect(devices, &QComboBox::currentIndexChanged, parent, [=] { refresh(); });
        enumerate();
        auto* apply = button(QObject::tr("Apply volume"));
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            Endpoint e(devices->currentData().toString());
            if (!e.volume) {
                status->setText(QObject::tr("No playback device."));
                return;
            }
            HRESULT hr = e.volume->SetMasterVolumeLevelScalar(volume->value() / 100.0f, nullptr);
            if (SUCCEEDED(hr))
                hr = e.volume->SetMute(mute->isChecked(), nullptr);
            if (FAILED(hr))
                result(parent, DWORD(hr));
            else
                status->setText(QObject::tr("Volume applied."));
        });
    } else if (category == 2) {
        auto* administrator = button(QObject::tr("Open these controls as administrator…"));
        QObject::connect(administrator, &QPushButton::clicked, parent, [=] {
            const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            const auto launched = ShellExecuteW(nullptr, L"runas", reinterpret_cast<LPCWSTR>(executable.utf16()),
                L"--page=network", nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(launched) <= 32)
                status->setText(QObject::tr("Administrator launch cancelled or unavailable."));
        });
        auto* adapter = new QComboBox(parent);
        auto* dns = new QLineEdit(parent);
        dns->setPlaceholderText(QObject::tr("IP addresses separated by commas; blank = DHCP"));
        form->addRow(QObject::tr("Adapter"), adapter);
        form->addRow(QObject::tr("DNS servers"), dns);
        auto* reload = button(QObject::tr("Refresh network adapters"));
        QObject::connect(reload, &QPushButton::clicked, parent, [=] {
            script(status, reload,
                "ConvertTo-Json -Compress -InputObject @(Get-NetAdapter | Select-Object Name,InterfaceIndex,Status)",
                [=](QByteArray data) {
                    adapter->clear();
                    for (auto v : QJsonDocument::fromJson(data).array()) {
                        auto o = v.toObject();
                        adapter->addItem(
                            o["Name"].toString() + " (" + o["Status"].toString() + ")", o["InterfaceIndex"].toInt());
                    }
                });
        });
        auto* apply = button(QObject::tr("Apply DNS (administrator permission may be required)"));
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            int id = adapter->currentData().toInt();
            if (id <= 0)
                return;
            QStringList addresses;
            for (auto address : dns->text().split(',', Qt::SkipEmptyParts)) {
                QHostAddress parsed(address.trimmed());
                if (parsed.isNull() || !parsed.scopeId().isEmpty()) {
                    status->setText(QObject::tr("Invalid DNS IP address."));
                    return;
                }
                addresses << "'" + parsed.toString() + "'";
            }
            script(status, apply,
                QStringLiteral("Set-DnsClientServerAddress -InterfaceIndex %1 ").arg(id)
                    + (addresses.isEmpty() ? "-ResetServerAddresses"
                                           : "-ServerAddresses @(" + addresses.join(',') + ")"));
        });
        auto* dhcp = button(QObject::tr("Use automatic IPv4 address (DHCP)"));
        QObject::connect(dhcp, &QPushButton::clicked, parent, [=] {
            int id = adapter->currentData().toInt();
            if (id <= 0)
                return;
            if (QMessageBox::question(parent, QObject::tr("Enable DHCP"),
                    QObject::tr("Replace the selected adapter's static IPv4 configuration?"))
                != QMessageBox::Yes)
                return;
            script(status, dhcp,
                QStringLiteral("Set-NetIPInterface -InterfaceIndex %1 -AddressFamily IPv4 -Dhcp Enabled").arg(id));
        });
        status->setText(QObject::tr("Refresh to select an adapter. DNS and DHCP changes are applied here; an elevated "
                                    "session is required on restricted accounts."));
    } else if (category == 3) {
        auto* radios = new QComboBox(parent);
        auto* visible = new QCheckBox(QObject::tr("Discoverable"), parent);
        auto* incoming = new QCheckBox(QObject::tr("Accept connections"), parent);
        form->addRow(QObject::tr("Bluetooth radio"), radios);
        form->addRow(incoming);
        form->addRow(visible);
        auto handles = std::make_shared<QList<HANDLE>>();
        QObject::connect(parent, &QObject::destroyed, [handles] {
            for (HANDLE h : *handles)
                CloseHandle(h);
        });
        auto* refresh = button(QObject::tr("Refresh radios"));
        QObject::connect(refresh, &QPushButton::clicked, parent, [=] {
            for (HANDLE h : *handles)
                CloseHandle(h);
            handles->clear();
            radios->clear();
            BLUETOOTH_FIND_RADIO_PARAMS p { sizeof(p) };
            HANDLE radio = nullptr;
            auto find = BluetoothFindFirstRadio(&p, &radio);
            if (find) {
                do {
                    BLUETOOTH_RADIO_INFO info {};
                    info.dwSize = sizeof(info);
                    BluetoothGetRadioInfo(radio, &info);
                    handles->append(radio);
                    radios->addItem(QString::fromWCharArray(info.szName));
                } while (BluetoothFindNextRadio(find, &radio));
                BluetoothFindRadioClose(find);
            }
            status->setText(handles->isEmpty()
                    ? QObject::tr("No Bluetooth radio found.")
                    : QObject::tr("Radio settings and classic Bluetooth pairing are managed here."));
        });
        QObject::connect(radios, &QComboBox::currentIndexChanged, parent, [=](int i) {
            if (i < 0 || i >= handles->size())
                return;
            visible->setChecked(BluetoothIsDiscoverable(handles->at(i)));
            incoming->setChecked(BluetoothIsConnectable(handles->at(i)));
        });
        auto* apply = button(QObject::tr("Apply radio settings"));
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            int i = radios->currentIndex();
            if (i < 0 || i >= handles->size())
                return;
            if (visible->isChecked() && !incoming->isChecked()) {
                status->setText(QObject::tr("Discoverability requires incoming connections."));
                return;
            }
            HANDLE h = handles->at(i);
            BOOL ok = TRUE;
            if (!visible->isChecked())
                ok = BluetoothEnableDiscovery(h, FALSE);
            if (ok)
                ok = BluetoothEnableIncomingConnections(h, incoming->isChecked());
            if (ok && visible->isChecked())
                ok = BluetoothEnableDiscovery(h, TRUE);
            if (!ok)
                result(parent, GetLastError());
            else
                status->setText(QObject::tr("Bluetooth settings applied."));
        });
        addBluetoothDevices(layout, radios, handles, status);
    } else if (category == 4) {
        auto* layouts = new QComboBox(parent);
        form->addRow(QObject::tr("Default input layout"), layouts);
        HKL list[256];
        int count = GetKeyboardLayoutList(256, list);
        for (int i = 0; i < count; ++i) {
            wchar_t language[128] {};
            GetLocaleInfoW(LOWORD(reinterpret_cast<ULONG_PTR>(list[i])), LOCALE_SLOCALIZEDDISPLAYNAME, language, 128);
            layouts->addItem(QString::fromWCharArray(language)
                    + QStringLiteral(" [%1]").arg(qulonglong(reinterpret_cast<ULONG_PTR>(list[i])), 0, 16),
                qulonglong(reinterpret_cast<ULONG_PTR>(list[i])));
        }
        auto* apply = button(QObject::tr("Set default input layout"));
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            HKL layout = reinterpret_cast<HKL>(quintptr(layouts->currentData().toULongLong()));
            if (!layout)
                return;
            if (!SystemParametersInfoW(SPI_SETDEFAULTINPUTLANG, 0, &layout, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
                result(parent, GetLastError());
            else {
                ActivateKeyboardLayout(layout, 0);
                status->setText(
                    QObject::tr("Default input layout saved. Existing applications may retain their active layout."));
            }
        });
        auto* displayLanguages = new QComboBox(parent);
        form->addRow(QObject::tr("Windows display language"), displayLanguages);
        auto* refreshLanguages = button(QObject::tr("Refresh installed display languages"));
        QObject::connect(refreshLanguages, &QPushButton::clicked, parent, [=] {
            script(status, refreshLanguages,
                QStringLiteral("ConvertTo-Json -Compress -InputObject @(Get-ChildItem "
                               "'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\MUI\\UILanguages' | ForEach-Object { "
                               "[pscustomobject]@{ID=$_.PSChildName;Name=[Globalization.CultureInfo]::GetCultureInfo($_"
                               ".PSChildName).NativeName} })"),
                [=](QByteArray bytes) {
                    displayLanguages->clear();
                    for (auto value : QJsonDocument::fromJson(bytes).array()) {
                        auto object = value.toObject();
                        displayLanguages->addItem(object["Name"].toString(), object["ID"].toString());
                    }
                });
        });
        auto* applyDisplayLanguage = button(QObject::tr("Set Windows display language for next sign-in"));
        QObject::connect(applyDisplayLanguage, &QPushButton::clicked, parent, [=] {
            const QString id = displayLanguages->currentData().toString();
            static const QRegularExpression valid("^[A-Za-z]{2,3}(?:-[A-Za-z0-9]{2,8})*$");
            if (!valid.match(id).hasMatch())
                return;
            script(status, applyDisplayLanguage, "Set-WinUILanguageOverride -Language '" + id + "'", [=](QByteArray) {
                status->setText(QObject::tr("Windows display language saved. Sign out and sign in to apply it."));
            });
        });
        status->setText(QObject::tr("Choose an installed Windows display language. New display-language packs and "
                                    "per-monitor display scaling require additional Windows support."));
    } else if (category == 5) {
        auto* preferences = new NotificationManager::Settings(parent);
        preferences->load();
        auto* dnd = new QComboBox(parent);
        dnd->addItem(QObject::tr("Off"), 0);
        dnd->addItem(QObject::tr("For one hour"), 60);
        dnd->addItem(QObject::tr("For eight hours"), 480);
        dnd->addItem(QObject::tr("Until tomorrow"), 1440);
        const auto currentUntil = preferences->notificationsInhibitedUntil();
        if (currentUntil > QDateTime::currentDateTime()) {
            dnd->addItem(QObject::tr("Until %1").arg(QLocale().toString(currentUntil, QLocale::ShortFormat)),
                int(qMax<qint64>(1, QDateTime::currentDateTime().secsTo(currentUntil) / 60)));
            dnd->setCurrentIndex(dnd->count() - 1);
        }
        auto* popupPosition = new QComboBox(parent);
        const char* positions[] = { QT_TRANSLATE_NOOP("QObject", "Near notification widget"),
            QT_TRANSLATE_NOOP("QObject", "Top left"), QT_TRANSLATE_NOOP("QObject", "Top center"),
            QT_TRANSLATE_NOOP("QObject", "Top right"), QT_TRANSLATE_NOOP("QObject", "Bottom left"),
            QT_TRANSLATE_NOOP("QObject", "Bottom center"), QT_TRANSLATE_NOOP("QObject", "Bottom right") };
        for (int i = 0; i < 7; ++i)
            popupPosition->addItem(QObject::tr(positions[i]), i);
        popupPosition->setCurrentIndex(int(preferences->popupPosition()));
        auto* timeout = new QSpinBox(parent);
        timeout->setRange(1, 120);
        timeout->setSuffix(QObject::tr(" seconds"));
        timeout->setValue(qBound(1, preferences->popupTimeout() / 1000, 120));
        auto* sounds = new QCheckBox(QObject::tr("Mute notification sounds"), parent);
        sounds->setChecked(preferences->notificationSoundsInhibited());
        auto* badges = new QCheckBox(QObject::tr("Show notification badges on task buttons"), parent);
        badges->setChecked(preferences->badgesInTaskManager());
        form->addRow(QObject::tr("Do Not Disturb"), dnd);
        form->addRow(QObject::tr("Popup position"), popupPosition);
        form->addRow(QObject::tr("Popup duration"), timeout);
        form->addRow(sounds);
        form->addRow(badges);
        auto* applyNotifications = button(QObject::tr("Apply notification preferences"));
        QObject::connect(applyNotifications, &QPushButton::clicked, parent, [=] {
            preferences->setNotificationsInhibitedUntil(dnd->currentData().toInt()
                    ? QDateTime::currentDateTime().addSecs(dnd->currentData().toInt() * 60)
                    : QDateTime());
            preferences->setPopupPosition(
                NotificationManager::Settings::PopupPosition(popupPosition->currentData().toInt()));
            preferences->setPopupTimeout(timeout->value() * 1000);
            preferences->setNotificationSoundsInhibited(sounds->isChecked());
            preferences->setBadgesInTaskManager(badges->isChecked());
            preferences->save();
            status->setText(QObject::tr("Notification preferences applied to Plasma."));
        });
        // Reflect changes made in the Plasma notification widget as well.
        auto* notificationRefresh = new QTimer(parent);
        QObject::connect(notificationRefresh, &QTimer::timeout, parent, [=] {
            const auto until = preferences->notificationsInhibitedUntil();
            const bool active = until > QDateTime::currentDateTime();
            applyNotifications->setToolTip(active
                    ? QObject::tr("Do Not Disturb until %1").arg(QLocale().toString(until, QLocale::ShortFormat))
                    : QObject::tr("Do Not Disturb is off"));
        });
        notificationRefresh->start(2000);

        auto* install = button(QObject::tr("Install / authorize Windows notification relay…"));
        QObject::connect(install, &QPushButton::clicked, parent, [] {
            QProcess::startDetached("cmd.exe",
                { "/c",
                    QDir::toNativeSeparators(
                        QCoreApplication::applicationDirPath() + "/../install-notifications.cmd") });
        });
        auto* open = button(QObject::tr("Open notification relay controls"));
        QObject::connect(open, &QPushButton::clicked, parent, [] {
            QProcess::startDetached(QCoreApplication::applicationDirPath() + "/plasma-notification-bridge.exe", {});
        });
        auto* test = button(QObject::tr("Send a KDE test notification"));
        QObject::connect(test, &QPushButton::clicked, parent, [=] {
            auto m = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications", "Notify");
            m << QString("System Settings") << uint(0) << QString("preferences-system")
              << QObject::tr("KDE notifications") << QObject::tr("Notification delivery test") << QStringList {}
              << QVariantMap {} << 5000;
            QDBusConnection::sessionBus().asyncCall(m);
            status->setText(QObject::tr("Test notification requested."));
        });
        status->setText(QObject::tr("Use the Plasma notification widget for history and Do Not Disturb. Windows "
                                    "reading permission is granted through the relay's authorization dialog."));
    } else if (category == 6) {
        wchar_t user[256] {};
        DWORD size = 256;
        GetUserNameW(user, &size);
        QString username = QString::fromWCharArray(user);
        const bool localAccount = currentLocalUser(username);
        form->addRow(QObject::tr("Current account"), new QLabel(username, parent));
        auto* fullName = new QLineEdit(parent);
        LPBYTE accountInfo = nullptr;
        if (localAccount
            && NetUserGetInfo(nullptr, reinterpret_cast<LPCWSTR>(username.utf16()), 2, &accountInfo) == NERR_Success) {
            auto* userInfo = reinterpret_cast<USER_INFO_2*>(accountInfo);
            fullName->setText(QString::fromWCharArray(userInfo->usri2_full_name));
            NetApiBufferFree(accountInfo);
        }
        form->addRow(QObject::tr("Account display name"), fullName);
        auto* saveName = button(QObject::tr("Save account display name"));
        saveName->setEnabled(localAccount);
        fullName->setEnabled(localAccount);
        QObject::connect(saveName, &QPushButton::clicked, parent, [=] {
            const QString name = fullName->text().trimmed();
            USER_INFO_1011 info {};
            info.usri1011_full_name = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(name.utf16()));
            const DWORD error = NetUserSetInfo(
                nullptr, reinterpret_cast<LPCWSTR>(username.utf16()), 1011, reinterpret_cast<LPBYTE>(&info), nullptr);
            if (error)
                result(parent, error);
            else
                status->setText(QObject::tr("Account display name saved."));
        });

        auto* old = new QLineEdit(parent);
        auto* fresh = new QLineEdit(parent);
        auto* confirm = new QLineEdit(parent);
        for (auto* field : { old, fresh, confirm })
            field->setEchoMode(QLineEdit::Password);
        form->addRow(QObject::tr("Current password"), old);
        form->addRow(QObject::tr("New password"), fresh);
        form->addRow(QObject::tr("Repeat password"), confirm);
        auto* apply = button(QObject::tr("Change local account password…"));
        apply->setEnabled(localAccount);
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            if (fresh->text() != confirm->text() || fresh->text().isEmpty()) {
                status->setText(QObject::tr("New passwords must match and be nonempty."));
                return;
            }
            DWORD code = NetUserChangePassword(nullptr, reinterpret_cast<LPCWSTR>(username.utf16()),
                reinterpret_cast<LPCWSTR>(old->text().utf16()), reinterpret_cast<LPCWSTR>(fresh->text().utf16()));
            old->clear();
            fresh->clear();
            confirm->clear();
            if (code)
                result(parent, code);
            else
                status->setText(QObject::tr("Password changed."));
        });
        status->setText(QObject::tr("Local account password only. Microsoft, domain and passwordless accounts may "
                                    "require their own identity provider."));
    } else if (category == 7) {
        auto* plans = new QComboBox(parent);
        auto* screen = new QSpinBox(parent);
        auto* sleep = new QSpinBox(parent);
        auto* screenDC = new QSpinBox(parent);
        auto* sleepDC = new QSpinBox(parent);
        screen->setRange(0, 1440);
        sleep->setRange(0, 1440);
        for (auto* s : { screen, sleep, screenDC, sleepDC }) {
            s->setRange(0, 1440);
            s->setSuffix(QObject::tr(" min"));
            s->setSpecialValueText(QObject::tr("Never"));
        }
        form->addRow(QObject::tr("Power plan"), plans);
        form->addRow(QObject::tr("Screen off on AC"), screen);
        form->addRow(QObject::tr("Sleep on AC"), sleep);
        form->addRow(QObject::tr("Screen off on battery"), screenDC);
        form->addRow(QObject::tr("Sleep on battery"), sleepDC);
        for (ULONG i = 0;; ++i) {
            GUID g {};
            DWORD bytes = sizeof(g);
            if (PowerEnumerate(nullptr, nullptr, nullptr, ACCESS_SCHEME, i, reinterpret_cast<UCHAR*>(&g), &bytes)
                != ERROR_SUCCESS)
                break;
            wchar_t name[512] {};
            bytes = sizeof(name);
            PowerReadFriendlyName(nullptr, &g, nullptr, nullptr, reinterpret_cast<UCHAR*>(name), &bytes);
            plans->addItem(QString::fromWCharArray(name), QByteArray(reinterpret_cast<char*>(&g), sizeof(g)));
        }
        auto load = [=] {
            auto bytes = plans->currentData().toByteArray();
            if (bytes.size() != sizeof(GUID))
                return;
            GUID g;
            memcpy(&g, bytes.constData(), sizeof(g));
            DWORD a = 0, b = 0;
            PowerReadACValueIndex(nullptr, &g, &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, &a);
            PowerReadACValueIndex(nullptr, &g, &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT, &b);
            screen->setValue(a / 60);
            sleep->setValue(b / 60);
            PowerReadDCValueIndex(nullptr, &g, &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, &a);
            PowerReadDCValueIndex(nullptr, &g, &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT, &b);
            screenDC->setValue(a / 60);
            sleepDC->setValue(b / 60);
        };
        QObject::connect(plans, &QComboBox::currentIndexChanged, parent, load);
        GUID* active = nullptr;
        if (PowerGetActiveScheme(nullptr, &active) == ERROR_SUCCESS) {
            QByteArray bytes(reinterpret_cast<char*>(active), sizeof(GUID));
            plans->setCurrentIndex(plans->findData(bytes));
            LocalFree(active);
        }
        load();
        auto* apply = button(QObject::tr("Apply power plan"));
        QObject::connect(apply, &QPushButton::clicked, parent, [=] {
            auto bytes = plans->currentData().toByteArray();
            if (bytes.size() != sizeof(GUID))
                return;
            GUID g;
            memcpy(&g, bytes.constData(), sizeof(g));
            struct Timeout {
                const GUID *group, *setting;
                bool dc;
                DWORD oldValue, newValue;
            };
            Timeout timeouts[]
                = { { &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, false, 0, DWORD(screen->value() * 60) },
                      { &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT, false, 0, DWORD(sleep->value() * 60) },
                      { &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, true, 0, DWORD(screenDC->value() * 60) },
                      { &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT, true, 0, DWORD(sleepDC->value() * 60) } };
            DWORD code = 0;
            for (auto& value : timeouts) {
                code = value.dc ? PowerReadDCValueIndex(nullptr, &g, value.group, value.setting, &value.oldValue)
                                : PowerReadACValueIndex(nullptr, &g, value.group, value.setting, &value.oldValue);
                if (code)
                    break;
            }
            if (code) {
                result(parent, code);
                return;
            }
            for (auto& value : timeouts) {
                code = value.dc ? PowerWriteDCValueIndex(nullptr, &g, value.group, value.setting, value.newValue)
                                : PowerWriteACValueIndex(nullptr, &g, value.group, value.setting, value.newValue);
                if (code)
                    break;
            }
            if (!code)
                code = PowerSetActiveScheme(nullptr, &g);
            if (code) {
                for (auto& value : timeouts) {
                    if (value.dc)
                        PowerWriteDCValueIndex(nullptr, &g, value.group, value.setting, value.oldValue);
                    else
                        PowerWriteACValueIndex(nullptr, &g, value.group, value.setting, value.oldValue);
                }
                result(parent, code);
            } else
                status->setText(QObject::tr("Power plan applied."));
        });
    } else if (category == 8) {
        addUpdateControls(layout, status);
    }
    layout->addWidget(status);
    layout->addStretch();
}
