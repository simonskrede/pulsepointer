#pragma once

#include "effect/effect.h"
#include "settings.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>

#include <array>
#include <memory>

class QByteArray;
class QPainter;
class QSocketNotifier;

namespace KWin
{

class ImageItem;
class Item;

class PulsePointerEffect : public Effect
{
    Q_OBJECT

public:
    PulsePointerEffect();
    ~PulsePointerEffect() override;

    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;

private:
    bool openSocket();
    void closeSocket();
    void readPendingPackets();
    bool applyTelemetry(const QByteArray &packet);
    void renderOverlay();
    void updatePosition(const QPointF &position);
    void refreshCursorImage();
    void rebuildRingImage();
    float targetCursorEnergy() const;
    void applyCursorTransform();
    void animateCursor();
    void updateVisibility();
    void setNativeCursorHidden(bool hidden);

    std::unique_ptr<ImageItem> m_overlayItem;
    std::unique_ptr<Item> m_ringRoot;
    std::unique_ptr<ImageItem> m_ringItem;
    std::unique_ptr<Item> m_cursorRoot;
    std::unique_ptr<ImageItem> m_cursorItem;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    QTimer m_staleTimer;
    QTimer m_animationTimer;
    QElapsedTimer m_lastPacketTimer;
    QElapsedTimer m_animationClock;
    QElapsedTimer m_ringClock;
    QString m_socketPath;
    PulsePointer::Settings m_settings;
    std::array<float, 32> m_spectrum{};
    std::array<float, 128> m_waveform{};
    quint32 m_renderedRingColor = 0;
    qreal m_renderedDevicePixelRatio = 0;
    float m_level = 0;
    float m_bass = 0;
    float m_midrange = 0;
    float m_treble = 0;
    float m_onsetPulse = 0;
    float m_lastOnset = 0;
    float m_currentEnergy = 0;
    float m_ringStrength = 0;
    qreal m_wobblePhase = 0;
    int m_socketFd = -1;
    bool m_havePacket = false;
    bool m_cursorImageAvailable = false;
    bool m_cursorHiddenByUs = false;
    bool m_screenLocked = false;
};

} // namespace KWin
