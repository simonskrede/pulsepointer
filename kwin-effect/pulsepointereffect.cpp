#include "pulsepointereffect.h"

#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTransform>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <numbers>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{

constexpr std::array<char, 4> s_magic{'P', 'P', 'A', 'U'};
constexpr quint16 s_protocolVersion = 2;
constexpr qsizetype s_headerSize = 16;
constexpr qsizetype s_summaryValues = 5;
constexpr qsizetype s_spectrumValues = 32;
constexpr qsizetype s_waveformValues = 128;
constexpr qsizetype s_packetSize = s_headerSize
    + (s_summaryValues + s_spectrumValues + s_waveformValues) * 4;
constexpr qsizetype s_spectrumOffset = s_headerSize + s_summaryValues * 4;
constexpr qsizetype s_waveformOffset = s_spectrumOffset + s_spectrumValues * 4;
constexpr int s_staleTimeoutMs = 1000;
constexpr int s_animationIntervalMs = 16;
constexpr qreal s_ringLogicalSize = 64;
constexpr qreal s_ringDurationMs = 420;

template<typename T>
T readLittleEndian(const char *data)
{
    return qFromLittleEndian<T>(reinterpret_cast<const uchar *>(data));
}

float readFloat(const char *data)
{
    return std::bit_cast<float>(readLittleEndian<quint32>(data));
}

bool isNormalized(float value)
{
    return std::isfinite(value) && value >= 0 && value <= 1;
}

bool isSignedNormalized(float value)
{
    return std::isfinite(value) && value >= -1 && value <= 1;
}

qreal visualizationAmplitude(const PulsePointer::Settings &settings)
{
    return settings.intensity / 100.0;
}

qreal elasticIntensity(const PulsePointer::Settings &settings)
{
    const qreal linearIntensity = visualizationAmplitude(settings);
    return linearIntensity <= 1.0
        ? linearIntensity
        : linearIntensity * linearIntensity;
}

bool isCursorMode(PulsePointer::VisualizationMode mode)
{
    return mode == PulsePointer::VisualizationMode::Elastic
        || mode == PulsePointer::VisualizationMode::Wobble;
}

void drawBar(QPainter &painter, const QRectF &bounds, float level, qreal amplitude)
{
    const qreal meterWidth = std::max<qreal>(4, bounds.width() * 0.10);
    const qreal meterHeight = bounds.height() * std::clamp(level * amplitude, 0.0, 1.0);
    const QRectF meter(bounds.right() - meterWidth - 2,
                       bounds.bottom() - meterHeight - 2,
                       meterWidth,
                       meterHeight);
    painter.drawRoundedRect(meter, meterWidth / 2, meterWidth / 2);
}

void drawSpectrum(QPainter &painter,
                  const QRectF &bounds,
                  const std::array<float, 32> &spectrum,
                  qreal amplitude)
{
    const qreal barWidth = bounds.width() / spectrum.size();
    for (std::size_t index = 0; index < spectrum.size(); ++index) {
        const qreal height = bounds.height()
            * std::clamp(spectrum[index] * amplitude, 0.0, 1.0);
        painter.drawRect(QRectF(index * barWidth,
                                bounds.bottom() - height,
                                std::max<qreal>(1, barWidth - 1),
                                height));
    }
}

void drawSpectrumHalo(QPainter &painter,
                      const QRectF &bounds,
                      const std::array<float, 32> &spectrum,
                      qreal amplitude)
{
    const qreal diameter = std::min(bounds.width(), bounds.height());
    const qreal innerRadius = diameter * 0.27;
    const qreal maximumLength = diameter * 0.20;
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bounds.center(), innerRadius, innerRadius);

    for (std::size_t index = 0; index < spectrum.size(); ++index) {
        const qreal angle = -std::numbers::pi / 2.0
            + index * 2.0 * std::numbers::pi / spectrum.size();
        const QPointF direction(std::cos(angle), std::sin(angle));
        const qreal length = maximumLength
            * std::clamp(spectrum[index] * amplitude, 0.0, 1.0);
        painter.drawLine(bounds.center() + direction * innerRadius,
                         bounds.center() + direction * (innerRadius + length));
    }
}

void drawOscilloscope(QPainter &painter,
                      const QRectF &bounds,
                      const std::array<float, 128> &waveform,
                      qreal amplitude)
{
    QPainterPath path;
    const qreal centerY = bounds.center().y();
    const qreal verticalScale = bounds.height() * 0.45 * amplitude;
    for (std::size_t index = 0; index < waveform.size(); ++index) {
        const qreal x = bounds.left()
            + index * bounds.width() / (waveform.size() - 1);
        const qreal y = centerY - std::clamp<qreal>(waveform[index], -1, 1) * verticalScale;
        if (index == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    painter.drawPath(path);
}

void drawCircle(QPainter &painter,
                const QRectF &bounds,
                const std::array<float, 128> &waveform,
                qreal amplitude)
{
    constexpr std::size_t pointCount = 64;
    QPainterPath path;
    for (std::size_t index = 0; index <= pointCount; ++index) {
        const std::size_t wrapped = index % pointCount;
        const qreal angle = wrapped * 2.0 * std::numbers::pi / pointCount;
        const qreal sample = waveform[wrapped * waveform.size() / pointCount];
        const qreal radius = bounds.width() * (0.30 + 0.10 * sample * amplitude);
        const QPointF point = bounds.center()
            + QPointF(std::cos(angle) * radius, std::sin(angle) * radius);
        if (index == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    painter.drawPath(path);
}

} // namespace

namespace KWin
{

PulsePointerEffect::PulsePointerEffect()
{
    Item *const overlayRoot = effects->scene()->overlayItem();
    ItemRenderer *const renderer = effects->scene()->renderer();

    m_overlayItem = renderer->createImageItem(overlayRoot);
    m_overlayItem->setVisible(false);

    m_ringRoot = std::make_unique<Item>(overlayRoot);
    m_ringRoot->setVisible(false);
    m_ringRoot->setZ(0);
    m_ringItem = renderer->createImageItem(m_ringRoot.get());

    m_cursorRoot = std::make_unique<Item>(overlayRoot);
    m_cursorRoot->setVisible(false);
    m_cursorRoot->setZ(1);
    m_cursorItem = renderer->createImageItem(m_cursorRoot.get());

    m_screenLocked = effects->isScreenLocked();

    connect(effects, &EffectsHandler::mouseChanged, this,
            [this](const QPointF &position, const QPointF &, Qt::MouseButtons, Qt::MouseButtons,
                   Qt::KeyboardModifiers, Qt::KeyboardModifiers) {
                updatePosition(position);
            });
    connect(effects, &EffectsHandler::cursorShapeChanged, this, [this] {
        if (isCursorMode(m_settings.mode)) {
            refreshCursorImage();
            updateVisibility();
        }
    });
    connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
        m_screenLocked = locked;
        updateVisibility();
    });

    m_staleTimer.setSingleShot(true);
    m_staleTimer.setInterval(s_staleTimeoutMs);
    connect(&m_staleTimer, &QTimer::timeout, this, [this] {
        m_havePacket = false;
        updateVisibility();
    });

    m_animationTimer.setTimerType(Qt::PreciseTimer);
    m_animationTimer.setInterval(s_animationIntervalMs);
    connect(&m_animationTimer, &QTimer::timeout, this, &PulsePointerEffect::animateCursor);

    reconfigure(ReconfigureAll);
    openSocket();
}

PulsePointerEffect::~PulsePointerEffect()
{
    setNativeCursorHidden(false);
    closeSocket();
}

bool PulsePointerEffect::isActive() const
{
    return (m_overlayItem && m_overlayItem->isVisible())
        || (m_cursorRoot && m_cursorRoot->isVisible())
        || (m_ringRoot && m_ringRoot->isVisible());
}

void PulsePointerEffect::reconfigure(ReconfigureFlags)
{
    const PulsePointer::Settings previous = m_settings;
    m_settings = PulsePointer::Settings::load();

    if (m_settings.color != previous.color) {
        rebuildRingImage();
    }
    if (isCursorMode(m_settings.mode)) {
        refreshCursorImage();
        applyCursorTransform();
    } else if (m_havePacket) {
        renderOverlay();
    }
    updatePosition(effects->cursorPos());
    updateVisibility();
}

bool PulsePointerEffect::openSocket()
{
    QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDirectory.isEmpty()) {
        runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    if (runtimeDirectory.isEmpty()) {
        qWarning("PulsePointer: no runtime directory is available");
        return false;
    }

    const QString socketDirectory = runtimeDirectory + QStringLiteral("/pulsepointer");
    if (!QDir().mkpath(socketDirectory)) {
        qWarning("PulsePointer: failed to create runtime socket directory");
        return false;
    }
    const QByteArray encodedDirectory = QFile::encodeName(socketDirectory);
    ::chmod(encodedDirectory.constData(), S_IRWXU);

    m_socketPath = socketDirectory + QStringLiteral("/kwin-effect.sock");
    const QByteArray encodedPath = QFile::encodeName(m_socketPath);
    if (encodedPath.size() >= qsizetype(sizeof(sockaddr_un::sun_path))) {
        qWarning("PulsePointer: runtime socket path is too long");
        m_socketPath.clear();
        return false;
    }

    m_socketFd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_socketFd < 0) {
        qWarning("PulsePointer: failed to create runtime socket: %s", std::strerror(errno));
        m_socketPath.clear();
        return false;
    }

    ::unlink(encodedPath.constData());
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, encodedPath.constData(), encodedPath.size() + 1);
    const socklen_t addressLength = offsetof(sockaddr_un, sun_path) + encodedPath.size() + 1;
    if (::bind(m_socketFd, reinterpret_cast<const sockaddr *>(&address), addressLength) != 0) {
        qWarning("PulsePointer: failed to bind runtime socket: %s", std::strerror(errno));
        closeSocket();
        return false;
    }
    ::chmod(encodedPath.constData(), S_IRUSR | S_IWUSR);

    m_socketNotifier = std::make_unique<QSocketNotifier>(m_socketFd, QSocketNotifier::Read, this);
    connect(m_socketNotifier.get(), &QSocketNotifier::activated, this,
            [this](QSocketDescriptor, QSocketNotifier::Type) {
                readPendingPackets();
            });
    return true;
}

void PulsePointerEffect::closeSocket()
{
    m_socketNotifier.reset();
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
    if (!m_socketPath.isEmpty()) {
        const QByteArray encodedPath = QFile::encodeName(m_socketPath);
        ::unlink(encodedPath.constData());
        m_socketPath.clear();
    }
}

void PulsePointerEffect::readPendingPackets()
{
    QByteArray latestPacket;
    QByteArray buffer(s_packetSize, Qt::Uninitialized);

    while (true) {
        const ssize_t received = ::recv(m_socketFd, buffer.data(), buffer.size(), MSG_DONTWAIT);
        if (received >= 0) {
            latestPacket = QByteArray(buffer.constData(), received);
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != EAGAIN) {
            qWarning("PulsePointer: failed to receive telemetry: %s", std::strerror(errno));
        }
        break;
    }

    if (!latestPacket.isEmpty()) {
        applyTelemetry(latestPacket);
    }
}

bool PulsePointerEffect::applyTelemetry(const QByteArray &packet)
{
    if (packet.size() != s_packetSize
        || !std::equal(s_magic.cbegin(), s_magic.cend(), packet.cbegin())) {
        return false;
    }

    const quint16 version = readLittleEndian<quint16>(packet.constData() + 4);
    const quint16 flags = readLittleEndian<quint16>(packet.constData() + 6);
    const float level = readFloat(packet.constData() + 16);
    const float bass = readFloat(packet.constData() + 20);
    const float midrange = readFloat(packet.constData() + 24);
    const float treble = readFloat(packet.constData() + 28);
    const float onset = readFloat(packet.constData() + 32);
    if (version != s_protocolVersion || flags != 0
        || !isNormalized(level) || !isNormalized(bass) || !isNormalized(midrange)
        || !isNormalized(treble) || !isNormalized(onset)) {
        return false;
    }

    std::array<float, s_spectrumValues> spectrum;
    for (qsizetype index = 0; index < s_spectrumValues; ++index) {
        spectrum[index] = readFloat(packet.constData() + s_spectrumOffset + index * 4);
        if (!isNormalized(spectrum[index])) {
            return false;
        }
    }
    std::array<float, s_waveformValues> waveform;
    for (qsizetype index = 0; index < s_waveformValues; ++index) {
        waveform[index] = readFloat(packet.constData() + s_waveformOffset + index * 4);
        if (!isSignedNormalized(waveform[index])) {
            return false;
        }
    }

    m_level = level;
    m_bass = bass;
    m_midrange = midrange;
    m_treble = treble;
    m_spectrum = spectrum;
    m_waveform = waveform;
    m_onsetPulse = std::max(m_onsetPulse, onset);

    if (m_settings.beatRing
        && isCursorMode(m_settings.mode)
        && onset >= 0.12f && m_lastOnset < 0.12f) {
        m_ringStrength = onset * (0.6f + 0.4f * treble);
        m_ringClock.restart();
    }
    m_lastOnset = onset;

    m_havePacket = true;
    m_lastPacketTimer.restart();
    m_staleTimer.start();
    if (isCursorMode(m_settings.mode)) {
        if (!m_cursorImageAvailable) {
            refreshCursorImage();
        }
    } else {
        renderOverlay();
    }
    updatePosition(effects->cursorPos());
    updateVisibility();
    return true;
}

void PulsePointerEffect::renderOverlay()
{
    const int logicalSize = m_settings.overlaySize;
    qreal devicePixelRatio = 1;
    const PlatformCursorImage cursor = effects->cursorImage();
    if (!cursor.isNull()) {
        devicePixelRatio = std::clamp(cursor.image().devicePixelRatio(), 1.0, 4.0);
    }
    const int pixelSize = qRound(logicalSize * devicePixelRatio);
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = QColor::fromRgba(m_settings.color);
    QPen pen(color);
    pen.setWidthF(std::max<qreal>(1.5, logicalSize / 32.0));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(color);

    const QRectF bounds(0, 0, logicalSize, logicalSize);
    const qreal amplitude = visualizationAmplitude(m_settings);
    switch (m_settings.mode) {
    case PulsePointer::VisualizationMode::Bar:
        drawBar(painter, bounds, m_level, amplitude);
        break;
    case PulsePointer::VisualizationMode::Circle:
        painter.setBrush(Qt::NoBrush);
        drawCircle(painter, bounds, m_waveform, amplitude);
        break;
    case PulsePointer::VisualizationMode::Spectrum:
        drawSpectrum(painter, bounds, m_spectrum, amplitude);
        break;
    case PulsePointer::VisualizationMode::Oscilloscope:
        painter.setBrush(Qt::NoBrush);
        drawOscilloscope(painter, bounds, m_waveform, amplitude);
        break;
    case PulsePointer::VisualizationMode::SpectrumHalo:
        drawSpectrumHalo(painter, bounds, m_spectrum, amplitude);
        break;
    case PulsePointer::VisualizationMode::Elastic:
    case PulsePointer::VisualizationMode::Wobble:
        break;
    }
    painter.end();

    m_overlayItem->setImage(image);
    m_overlayItem->setSize(QSizeF(logicalSize, logicalSize));
}

void PulsePointerEffect::updatePosition(const QPointF &position)
{
    const qreal halfOverlay = m_settings.overlaySize / 2.0;
    m_overlayItem->setPosition(position - QPointF(halfOverlay, halfOverlay));
    m_cursorRoot->setPosition(position);
    m_ringRoot->setPosition(position);
}

void PulsePointerEffect::refreshCursorImage()
{
    const PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.isNull()) {
        m_cursorImageAvailable = false;
        m_cursorRoot->setVisible(false);
        setNativeCursorHidden(false);
        return;
    }

    const QImage image = cursor.image();
    m_cursorItem->setImage(image);
    m_cursorItem->setPosition(-cursor.hotSpot());
    m_cursorItem->setSize(image.deviceIndependentSize());
    m_cursorImageAvailable = true;

    const qreal devicePixelRatio = std::clamp(image.devicePixelRatio(), 1.0, 4.0);
    if (m_renderedRingColor != m_settings.color
        || !qFuzzyCompare(m_renderedDevicePixelRatio, devicePixelRatio)) {
        m_renderedDevicePixelRatio = devicePixelRatio;
        rebuildRingImage();
    }
}

void PulsePointerEffect::rebuildRingImage()
{
    if (m_settings.color == 0 || m_renderedDevicePixelRatio <= 0) {
        return;
    }

    const int pixelSize = qRound(s_ringLogicalSize * m_renderedDevicePixelRatio);
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(m_renderedDevicePixelRatio);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor::fromRgba(m_settings.color));
    pen.setWidthF(2.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(3, 3, s_ringLogicalSize - 6, s_ringLogicalSize - 6));
    painter.end();

    m_ringItem->setImage(image);
    m_ringItem->setSize(QSizeF(s_ringLogicalSize, s_ringLogicalSize));
    m_ringItem->setPosition(QPointF(-s_ringLogicalSize / 2, -s_ringLogicalSize / 2));
    m_renderedRingColor = m_settings.color;
}

float PulsePointerEffect::targetCursorEnergy() const
{
    return 0.25f * m_level + 0.65f * m_bass + 0.10f * m_midrange;
}

void PulsePointerEffect::applyCursorTransform()
{
    const qreal intensity = elasticIntensity(m_settings);
    if (m_settings.mode == PulsePointer::VisualizationMode::Wobble) {
        const qreal activity = std::clamp<qreal>(
            m_currentEnergy * 1.5 + m_onsetPulse, 0, 1);
        const qreal wave = std::sin(m_wobblePhase);
        const qreal angle = std::clamp(
            intensity * activity
                * (3.0 + 8.0 * m_currentEnergy + 5.0 * m_onsetPulse) * wave,
            -32.0,
            32.0);
        const qreal horizontalScale = std::clamp(
            1.0 + intensity
                * (0.10 * m_currentEnergy + 0.04 * m_onsetPulse
                   + 0.04 * wave * m_currentEnergy),
            0.65,
            2.20);
        const qreal verticalScale = std::clamp(
            1.0 + intensity
                * (0.04 * m_currentEnergy - 0.08 * m_onsetPulse
                   - 0.03 * wave * m_currentEnergy),
            0.60,
            1.80);
        QTransform transform;
        transform.rotate(angle);
        transform.scale(horizontalScale, verticalScale);
        m_cursorRoot->setTransform(transform);
        return;
    }

    const qreal baseScale = 1.0
        + intensity * (0.12 * m_currentEnergy + 0.06 * m_onsetPulse);
    const qreal distortion = intensity
        * (0.08 * m_currentEnergy + 0.10 * m_onsetPulse);
    const qreal horizontalScale = std::clamp(
        baseScale * (1.0 + distortion), 0.55, 2.75);
    const qreal verticalScale = std::clamp(
        baseScale * (1.0 - distortion * 0.80), 0.55, 2.25);
    m_cursorRoot->setTransform(QTransform::fromScale(horizontalScale, verticalScale));
}

void PulsePointerEffect::animateCursor()
{
    const bool fresh = m_havePacket && m_lastPacketTimer.isValid()
        && m_lastPacketTimer.elapsed() < s_staleTimeoutMs;
    if (!isCursorMode(m_settings.mode) || !fresh || m_screenLocked) {
        updateVisibility();
        return;
    }

    const qreal elapsedMs = m_animationClock.isValid()
        ? std::clamp<qreal>(m_animationClock.restart(), 1, 50)
        : s_animationIntervalMs;
    const qreal elapsedSeconds = elapsedMs / 1000.0;
    const float targetEnergy = targetCursorEnergy();
    const qreal responsiveness = m_settings.responsiveness / 100.0;
    const qreal timeConstant = (targetEnergy > m_currentEnergy ? 0.045 : 0.180)
        / responsiveness;
    const qreal response = 1.0 - std::exp(-elapsedSeconds / timeConstant);
    m_currentEnergy += (targetEnergy - m_currentEnergy) * response;
    m_onsetPulse *= std::exp(-elapsedSeconds / (0.170 / responsiveness));
    if (m_settings.mode == PulsePointer::VisualizationMode::Wobble) {
        m_wobblePhase = std::fmod(
            m_wobblePhase + elapsedSeconds * (4.0 + 5.0 * m_bass + 9.0 * m_treble),
            2.0 * std::numbers::pi);
    }

    applyCursorTransform();

    bool ringActive = false;
    if (m_settings.beatRing && m_ringClock.isValid()) {
        const qreal progress = m_ringClock.elapsed() / s_ringDurationMs;
        if (progress < 1.0) {
            ringActive = true;
            const qreal scale = 0.35 + progress * 0.95;
            const qreal fade = 1.0 - progress;
            m_ringRoot->setTransform(QTransform::fromScale(scale, scale));
            m_ringRoot->setOpacity(m_ringStrength * fade * fade);
            m_ringRoot->setVisible(true);
        } else {
            m_ringClock.invalidate();
            m_ringRoot->setVisible(false);
        }
    }

    const bool wobbleActive = m_settings.mode == PulsePointer::VisualizationMode::Wobble
        && (targetEnergy >= 0.005 || m_currentEnergy >= 0.005);
    if (std::abs(targetEnergy - m_currentEnergy) < 0.001
        && m_onsetPulse < 0.002
        && !wobbleActive
        && !ringActive) {
        m_animationTimer.stop();
    }
}

void PulsePointerEffect::updateVisibility()
{
    const bool fresh = m_havePacket && m_lastPacketTimer.isValid()
        && m_lastPacketTimer.elapsed() < s_staleTimeoutMs;
    const bool visible = fresh && !m_screenLocked;
    const bool cursorMode = isCursorMode(m_settings.mode);
    const bool cursorVisible = visible && cursorMode && m_cursorImageAvailable;

    m_overlayItem->setVisible(visible && !cursorMode);
    m_cursorRoot->setVisible(cursorVisible);
    setNativeCursorHidden(cursorVisible);

    if (!m_settings.beatRing) {
        m_ringClock.invalidate();
        m_ringRoot->setVisible(false);
    }

    if (cursorVisible) {
        const bool ringActive = m_settings.beatRing && m_ringClock.isValid()
            && m_ringClock.elapsed() < s_ringDurationMs;
        const float targetEnergy = targetCursorEnergy();
        const bool wobbleActive = m_settings.mode == PulsePointer::VisualizationMode::Wobble
            && (targetEnergy >= 0.005 || m_currentEnergy >= 0.005);
        const bool animationNeeded = std::abs(targetEnergy - m_currentEnergy) >= 0.001
            || m_onsetPulse >= 0.002
            || wobbleActive
            || ringActive;
        if (animationNeeded && !m_animationTimer.isActive()) {
            m_animationClock.restart();
            m_animationTimer.start();
        }
    } else {
        m_animationTimer.stop();
        m_ringRoot->setVisible(false);
        m_cursorRoot->setTransform(QTransform());
    }
}

void PulsePointerEffect::setNativeCursorHidden(bool hidden)
{
    if (hidden == m_cursorHiddenByUs) {
        return;
    }
    if (hidden) {
        effects->hideCursor();
    } else {
        effects->showCursor();
    }
    m_cursorHiddenByUs = hidden;
}

} // namespace KWin

#include "moc_pulsepointereffect.cpp"
