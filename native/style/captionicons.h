// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <KIconLoader>
#include <cmath>

inline double captionLuminance(const QColor& color)
{
    auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

inline double captionContrast(const QColor& a, const QColor& b)
{
    const double x = captionLuminance(a), y = captionLuminance(b);
    return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}

inline QIcon captionButtonIcon(const QString& name, const QPalette& palette)
{
    // Use the Breeze glyph's alpha mask, not its cached global theme color.
    // A foreign application's palette can differ from the icon loader's.
    const auto background = palette.color(QPalette::Window);
    QColor foreground = palette.color(QPalette::WindowText);
    foreground.setAlpha(255);
    if (captionContrast(foreground, background) < 4.5)
        foreground = captionContrast(Qt::black, background) >= captionContrast(Qt::white, background)
            ? QColor(Qt::black)
            : QColor(Qt::white);
    QIcon icon;
    for (int size : { 16, 32 }) {
        auto glyph = KIconLoader::global()->loadIcon(name, KIconLoader::Toolbar, size);
        if (glyph.isNull())
            continue;
        QPainter painter(&glyph);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(glyph.rect(), foreground);
        painter.end();
        // Explicit modes avoid Qt synthesizing faint/white disabled glyphs.
        for (auto mode : { QIcon::Normal, QIcon::Active, QIcon::Disabled, QIcon::Selected })
            icon.addPixmap(glyph, mode);
    }
    return icon;
}
