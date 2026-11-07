#pragma once

#include "paintcompat.h"
#include "settings.h"
#include "musicmotion.h"

#include <tuple>
#include <chrono>

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
class ScreenRenderer;

class PulsePointerEffect : public Effect
{
    Q_OBJECT

public:
    PulsePointerEffect();
    ~PulsePointerEffect() override;

    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;
#if PULSEPOINTER_PREPAINT_PRESENT_TIME
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
#else
    void prePaintScreen(ScreenPrePaintData &data) override;
#endif
    ScreenPaint::Result paintScreen(const RenderTarget &target, const RenderViewport &viewport,
                                    int mask, std::tuple_element_t<3, ScreenPaint::Arguments> region,
                                    std::tuple_element_t<4, ScreenPaint::Arguments> screen) override;
    bool blocksDirectScanout() const override;
    int requestedEffectChainPosition() const override { return 99; }

private:
    bool screenEffectsActive() const;
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

    std::unique_ptr<ScreenRenderer> m_screenRenderer;
    PulsePointer::MusicMotion m_motion;
    bool m_bypassed = false;
    bool m_visualsVisible = false;
    std::unique_ptr<ImageItem> m_overlayItem;
    std::unique_ptr<Item> m_ringRoot;
    std::unique_ptr<ImageItem> m_ringItem;
    std::unique_ptr<Item> m_cursorRoot;
    std::unique_ptr<ImageItem> m_cursorItem;
    std::unique_ptr<ImageItem> m_hotspotLocatorItem;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    QTimer m_staleTimer;
    QTimer m_animationTimer;
    QElapsedTimer m_lastPacketTimer;
    QElapsedTimer m_animationClock;
    QElapsedTimer m_ringClock;
    QString m_socketPath;
    PulsePointer::Settings m_settings;
    QPointF m_cursorHotSpot;
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
