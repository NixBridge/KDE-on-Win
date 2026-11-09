// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QMetaObject>
#include <functional>
#include <mutex>

// Invalidate on the receiver's thread before destruction. The mutex serializes
// that operation with posting from WinRT callbacks; queued work is owned by Qt's
// receiver context and is discarded when the receiver is destroyed.
class AsyncCallbackTarget {
public:
    explicit AsyncCallbackTarget(QObject* receiver)
        : target(receiver)
    {
    }
    void invalidate()
    {
        std::lock_guard<std::mutex> lock(mutex);
        target = nullptr;
    }
    bool post(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return target && QMetaObject::invokeMethod(target, std::move(callback), Qt::QueuedConnection);
    }

private:
    std::mutex mutex;
    QObject* target;
};
