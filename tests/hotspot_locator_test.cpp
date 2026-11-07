#include "cursorgeometry.h"
#include "hotspotlocator.h"

#include <QtTest>

class HotspotLocatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void redCircleAcrossScales()
    {
        const QImage marker = PulsePointer::hotspotLocatorImage();
        const QSizeF size = marker.deviceIndependentSize();
        QCOMPARE(size, QSizeF(5, 5));
        QCOMPARE(marker.pixelColor(marker.width() / 2, marker.height() / 2), QColor(Qt::red));
        for (const QPoint corner : {QPoint(0, 0), QPoint(marker.width() - 1, 0),
                                    QPoint(0, marker.height() - 1),
                                    QPoint(marker.width() - 1, marker.height() - 1)}) {
            QCOMPARE(marker.pixelColor(corner).alpha(), 0);
        }
        const QPointF center = QRectF(QPointF(), size).center();
        const QTransform transform = PulsePointer::cursorImageTransform(center, QTransform());
        QCOMPARE(transform.map(center), QPointF());

        for (qreal scale : {0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0}) {
            // Include subpixel placement, output edges/corners, and a monitor
            // with a negative global origin. Even a clipped circle should
            // retain an opaque red center and have no border colors.
            const QPointF origin(-800.25, -200.5);
            for (const QPointF local : {QPointF(16.25, 16.75), QPointF(0, 0),
                                        QPointF(32, 0), QPointF(0, 32), QPointF(32, 32)}) {
                const QPointF global = origin + local;
                const QTransform scene = transform
                    * QTransform::fromTranslate(global.x(), global.y());
                QVERIFY(QLineF(scene.map(center), global).length() < 1e-9);

                QImage overlay(qRound(32 * scale), qRound(32 * scale), QImage::Format_ARGB32_Premultiplied);
                overlay.fill(Qt::transparent);
                {
                    QPainter painter(&overlay);
                    painter.setRenderHint(QPainter::SmoothPixmapTransform);
                    painter.scale(scale, scale);
                    painter.translate(-origin);
                    painter.setTransform(scene, true);
                    painter.drawImage(QPointF(), marker);
                }
                int opaqueRed = 0;
                for (int y = 0; y < overlay.height(); ++y) {
                    for (int x = 0; x < overlay.width(); ++x) {
                        const QColor color = overlay.pixelColor(x, y);
                        if (color.alpha() == 0) continue;
                        QCOMPARE(color.red(), 255);
                        QCOMPARE(color.green(), 0);
                        QCOMPARE(color.blue(), 0);
                        opaqueRed += color.alpha() >= 250;
                    }
                }
                const QByteArray context = QStringLiteral("scale=%1 position=(%2,%3)")
                    .arg(scale).arg(local.x()).arg(local.y()).toUtf8();
                QVERIFY2(opaqueRed > 0, context.constData());
            }
        }
    }
};

QTEST_GUILESS_MAIN(HotspotLocatorTest)
#include "hotspot_locator_test.moc"
