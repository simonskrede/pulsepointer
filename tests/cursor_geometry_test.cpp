#include "cursorgeometry.h"

#include <QLineF>
#include <QtTest>

#include <array>

class CursorGeometryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hotspotRemainsAtPointer()
    {
        // Include centered and asymmetric hotspots, fractional scale factors,
        // and negative screen coordinates used by multi-monitor layouts.
        const std::array hotspots{QPointF(0, 0), QPointF(3, 5), QPointF(16, 16), QPointF(31, 7)};
        const std::array positions{QPointF(100, 200), QPointF(-800.25, 70.5)};
        for (qreal dpr : {1.0, 1.25, 1.5, 2.0, 3.0}) {
            for (const QPointF &physicalHotspot : hotspots) {
                const QPointF hotspot = physicalHotspot / dpr;
                for (qreal angle : {-32.0, -12.0, 0.0, 12.0, 32.0}) {
                    for (const QSizeF scale : {QSizeF(1, 1), QSizeF(0.55, 2.25), QSizeF(2.75, 0.55)}) {
                        QTransform animation;
                        animation.rotate(angle);
                        animation.scale(scale.width(), scale.height());
                        const QTransform image = PulsePointer::cursorImageTransform(hotspot, animation);
                        for (const QPointF &position : positions) {
                            const QTransform scene = image
                                * QTransform::fromTranslate(position.x(), position.y());
                            QVERIFY2(QLineF(scene.map(hotspot), position).length() < 1e-9,
                                     "Animation moved the cursor away from its click position");
                        }
                    }
                }
            }
        }
    }

    void imageStillRotatesAndScales()
    {
        const QPointF hotspot(3.5, 7.25);
        QTransform animation;
        animation.rotate(90);
        animation.scale(2, 0.5);
        const QTransform image = PulsePointer::cursorImageTransform(hotspot, animation);
        // A point ten pixels to the right of the hotspot should move twenty
        // pixels down; a point ten pixels below should move five pixels left.
        QVERIFY(QLineF(image.map(hotspot + QPointF(10, 0)), QPointF(0, 20)).length() < 1e-9);
        QVERIFY(QLineF(image.map(hotspot + QPointF(0, 10)), QPointF(-5, 0)).length() < 1e-9);
    }
};

QTEST_GUILESS_MAIN(CursorGeometryTest)

#include "cursor_geometry_test.moc"
