// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationrelay.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QVariantMap>

static constexpr auto service = "org.freedesktop.Notifications";
static QDBusMessage method(const QString &name)
{
    return QDBusMessage::createMethodCall(QLatin1String(service), QStringLiteral("/org/freedesktop/Notifications"), QLatin1String(service), name);
}
NotificationRelay::NotificationRelay(QObject *parent, const QDBusConnection &bus) : QObject(parent), m_bus(bus)
{
    m_bus.connect(QLatin1String(service), QStringLiteral("/org/freedesktop/Notifications"), QLatin1String(service), QStringLiteral("ActionInvoked"), this, SLOT(actionInvoked(uint,QString)));
    m_bus.connect(QLatin1String(service), QStringLiteral("/org/freedesktop/Notifications"), QLatin1String(service), QStringLiteral("NotificationClosed"), this, SLOT(notificationClosed(uint,uint)));
    auto *watcher = new QDBusServiceWatcher(QLatin1String(service), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &oldOwner, const QString &newOwner) {
        if (oldOwner == newOwner) return;
        if (oldOwner.isEmpty()) {
            for (const auto &entry : m_entries) if (entry->active && !entry->busy && !entry->id) send(entry);
            return;
        }
        ++m_generation;
        // IDs belong to one notification server instance only.
        for (auto &entry : m_entries) entry->active = false;
        m_entries.clear(); m_ids.clear();
    });
}
NotificationRelay::~NotificationRelay()
{
    for(uint id:m_ids.keys()) close(id);
}
void NotificationRelay::publish(const QString &key, const QString &app, const QString &title, const QString &body,
                                const QString &icon, bool clickable, uchar urgency, int timeout)
{
    auto &entry = m_entries[key];
    if (!entry) { entry.reset(new Entry); entry->key = key; entry->generation = m_generation; }
    entry->app = app; entry->title = title; entry->body = body; entry->icon = icon;
    entry->clickable = clickable; entry->urgency = qMin(urgency, uchar(2)); entry->timeout = timeout;
    entry->dirty = true;
    if (!entry->busy) send(entry);
}
void NotificationRelay::send(const QSharedPointer<Entry> &entry)
{
    entry->busy = true; entry->dirty = false;
    auto request = method(QStringLiteral("Notify"));
    request.setAutoStartService(false);
    QVariantMap hints{{QStringLiteral("urgency"), QVariant::fromValue(entry->urgency)},
                      {QStringLiteral("suppress-sound"), true}};
    const QString body = entry->body.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    const QStringList actions = entry->clickable ? QStringList{QStringLiteral("default"), tr("Open")} : QStringList{};
    request.setArguments({entry->app, entry->id, entry->icon, entry->title, body, actions, hints, entry->timeout});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, entry] {
        QDBusPendingReply<uint> reply = *watcher;
        watcher->deleteLater(); entry->busy = false;
        // Never close a new server's notification using an old server's ID.
        if(entry->generation != m_generation) return;
        if (reply.isError()) {
            if (entry->active) Q_EMIT failed(entry->key, reply.error().message());
        } else if (!entry->active) {
            close(reply.value());
        } else {
            m_ids.remove(entry->id); entry->id = reply.value(); m_ids.insert(entry->id, entry->key);
            Q_EMIT delivered(entry->key);
        }
        if (entry->active && entry->dirty) send(entry);
    });
}
void NotificationRelay::close(uint id)
{
    if (!id) return;
    auto message = method(QStringLiteral("CloseNotification")); message << id;
    m_bus.asyncCall(message, 5000);
}
void NotificationRelay::withdraw(const QString &key)
{
    auto entry = m_entries.take(key);
    if (!entry) return;
    entry->active = false; m_ids.remove(entry->id); close(entry->id);
}
void NotificationRelay::actionInvoked(uint id, const QString &action)
{
    if (action == QLatin1String("default") && m_ids.contains(id)) Q_EMIT activated(m_ids.value(id));
}
void NotificationRelay::notificationClosed(uint id, uint reason)
{
    if (!m_ids.contains(id)) return;
    const QString key = m_ids.take(id);
    auto entry = m_entries.value(key);
    if (!entry || entry->id != id) return;
    entry->id = 0;
    // A replacement in flight still owns the key.
    if (!entry->busy) m_entries.remove(key);
    Q_EMIT dismissed(key, reason);
}
