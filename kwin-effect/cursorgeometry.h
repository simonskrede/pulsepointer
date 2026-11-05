#pragma once

#include <QPointF>
#include <QTransform>

namespace PulsePointer
{

inline QTransform cursorImageTransform(const QPointF &hotSpot, const QTransform &animation)
{
    // Put the image's click point at the origin before animating it. Keep the
    // offset in the transform so fractional hotspots are not snapped separately
    // as an item position by the renderer.
    return QTransform::fromTranslate(-hotSpot.x(), -hotSpot.y()) * animation;
}

} // namespace PulsePointer
