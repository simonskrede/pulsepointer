#pragma once

#include "musicmotion.h"
#include "settings.h"
#include <QElapsedTimer>
#include <QPointF>
#include <memory>

namespace KWin
{
class RenderTarget;
class RenderViewport;
class LogicalOutput;

class ScreenRenderer
{
public:
    ScreenRenderer();
    ~ScreenRenderer();
    void invalidateHistory(); // No GL calls; safe from input and audio callbacks.
    bool render(const RenderTarget &target, const RenderViewport &viewport, LogicalOutput *screen,
                const PulsePointer::Settings &settings, const PulsePointer::MusicMotion &motion,
                const std::array<float, 32> &spectrum, const std::array<float, 128> &waveform,
                const QPointF &cursor);
    bool available() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace KWin
