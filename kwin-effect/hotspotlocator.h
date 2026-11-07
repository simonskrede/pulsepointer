#pragma once

#include <QImage>
#include <QPainter>

namespace PulsePointer
{

inline QImage hotspotLocatorImage()
{
    // Logical dimensions follow the output scale, not the cursor theme size or
    // animation. Oversample the small texture to keep its curved edge smooth on
    // fractional and high-DPI outputs without rebuilding it between monitors.
    constexpr int size = 5;
    constexpr int textureScale = 4;
    QImage image(size * textureScale, size * textureScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(textureScale);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::red);
    painter.drawEllipse(QRectF(0, 0, size, size));
    painter.end();
    return image;
}

} // namespace PulsePointer
