// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMimeData>
#include <QStringList>
#include <QUrl>
#include <QtEndian>

class ClipboardHistory {
public:
    static bool permitted(const QMimeData* data)
    {
        if (!data || !data->hasText())
            return false;
        for (const QString& format : data->formats()) {
            if (format.contains("ExcludeClipboardContentFromMonitorProcessing", Qt::CaseInsensitive)
                || format == "application/x-kde-on-win-history")
                return false;
            // Treat every password-manager hint conservatively, including
            // empty, malformed or NUL-padded metadata, without reading payloads.
            if (format.contains("passwordManagerHint", Qt::CaseInsensitive))
                return false;
            if (format.contains("CanIncludeInClipboardHistory", Qt::CaseInsensitive)) {
                const auto value = data->data(format);
                // Malformed explicit privacy hints are treated conservatively.
                if (value.size() != 4 || qFromLittleEndian<quint32>(value.constData()) == 0)
                    return false;
            }
        }
        if (data->hasUrls())
            for (const auto& url : data->urls())
                if (url.isLocalFile())
                    return false;
        return true;
    }
    bool capture(const QMimeData* data)
    {
        if (!enabled || !permitted(data))
            return false;
        const auto value = data->text();
        if (value.isEmpty() || value.size() > 65536)
            return false;
        if (!entries.isEmpty() && entries.first() == value)
            return false;
        entries.removeAll(value);
        entries.prepend(value);
        trim();
        return true;
    }
    void setLimit(int count)
    {
        limit = qBound(1, count, 200);
        trim();
    }
    void clear() { entries.clear(); }
    void setEnabled(bool value)
    {
        enabled = value;
        if (!value)
            clear();
    }
    QStringList entries;
    int limit = 20;
    bool enabled = true;

private:
    void trim()
    {
        while (entries.size() > limit)
            entries.removeLast();
    }
};
