// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringConverter>
#include <QString>
#include <QSaveFile>

struct TextFormat {
    QStringConverter::Encoding encoding = QStringConverter::Utf8;
    bool bom = false;
    bool crlf = true;
};
inline bool decodeText(const QByteArray &bytes, QString &text, TextFormat &format)
{
    format = {};
    QByteArray content = bytes;
    if (content.startsWith("\xef\xbb\xbf")) { format.bom = true; content.remove(0, 3); }
    else if (content.startsWith("\xff\xfe")) { format.encoding = QStringConverter::Utf16LE; format.bom = true; content.remove(0, 2); }
    else if (content.startsWith("\xfe\xff")) { format.encoding = QStringConverter::Utf16BE; format.bom = true; content.remove(0, 2); }
    QStringDecoder decoder(format.encoding, QStringConverter::Flag::Stateless);
    text = decoder(content);
    if (decoder.hasError() || text.contains(QChar(0))) return false;
    format.crlf = text.contains(QStringLiteral("\r\n"));
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return true;
}
inline QByteArray encodeText(QString text, const TextFormat &format)
{
    if (format.crlf) text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
    QStringEncoder encoder(format.encoding);
    QByteArray result = encoder(text);
    if (format.bom) {
        if (format.encoding == QStringConverter::Utf8) result.prepend("\xef\xbb\xbf");
        if (format.encoding == QStringConverter::Utf16LE) result.prepend("\xff\xfe");
        if (format.encoding == QStringConverter::Utf16BE) result.prepend("\xfe\xff");
    }
    return result;
}
inline bool writeTextFile(const QString &path, const QString &text, const TextFormat &format, QString &error)
{
    QSaveFile file(path); file.setDirectWriteFallback(false);
    const QByteArray bytes = encodeText(text, format);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}
