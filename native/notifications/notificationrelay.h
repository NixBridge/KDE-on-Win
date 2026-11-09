// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QHash>
#include <QSharedPointer>
#include <QDBusConnection>

class NotificationRelay : public QObject
{
    Q_OBJECT
public:
    explicit NotificationRelay(QObject *parent = nullptr, const QDBusConnection &bus = QDBusConnection::sessionBus());
    ~NotificationRelay() override;
    void publish(const QString &key, const QString &app, const QString &title, const QString &body,
                 const QString &icon = QStringLiteral("dialog-information"), bool clickable = false,
                 uchar urgency = 1, int timeout = 8000);
    void withdraw(const QString &key);
Q_SIGNALS:
    void delivered(const QString &key);
    void activated(const QString &key);
    void dismissed(const QString &key, uint reason);
    void failed(const QString &key, const QString &error);
private Q_SLOTS:
    void actionInvoked(uint id, const QString &action);
    void notificationClosed(uint id, uint reason);
private:
    struct Entry {
        QString key, app, title, body, icon;
        uint id = 0;
        bool clickable = false, busy = false, dirty = false, active = true;
        uchar urgency = 1;
        int timeout = 8000;
        quint64 generation = 0;
    };
    void send(const QSharedPointer<Entry> &entry);
    void close(uint id);
    quint64 m_generation = 1;
    QDBusConnection m_bus;
    QHash<QString, QSharedPointer<Entry>> m_entries;
    QHash<uint, QString> m_ids;
};
