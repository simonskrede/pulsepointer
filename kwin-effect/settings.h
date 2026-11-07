#pragma once

#include "effectcatalog.h"
#include <QtGlobal>

namespace PulsePointer
{

// Retain the original values for migration and the pointer-overlay drawing code.
enum class VisualizationMode {
    Elastic = 0, Bar = 1, Circle = 2, Spectrum = 3, Oscilloscope = 4,
    SpectrumHalo = 5, Wobble = 6,
};
enum class CursorAnimation { None = 0, Elastic = 1, Wobble = 2 };
enum class MonitorScope { All = 0, Pointer = 1 };
enum class Preset { Velvet, Voltage, Underwater, Aurora, Meltdown };

constexpr quint32 pointerVisualizationMask = 0x1f;
constexpr quint32 pointerBit(VisualizationMode mode)
{
    const int value = static_cast<int>(mode);
    return value >= 1 && value <= 5 ? quint32(1) << (value - 1) : 0;
}

struct Settings
{
    bool enabled = true;
    CursorAnimation cursor = CursorAnimation::Elastic;
    quint32 pointerVisualizations = 0;
    int intensity = 100;
    bool beatRing = true;
    bool hotspotLocator = false;
    quint32 color = 0xCCFF4500;
    int overlaySize = 48;
    int responsiveness = 100;
    std::array<int, screenEffectCount> screenEffects{};
    int screenIntensity = 100;
    int edgeReach = 100;
    int edgeDensity = 16;
    int edgeSoftness = 65;
    int edgeProfile = 0; // natural, sine, rounded, triangle, spikes
    int attackMs = 45;
    int releaseMs = 350;
    MonitorScope monitors = MonitorScope::All;

    static Settings defaults();
    static Settings load();
    void save() const;
    void applyPreset(Preset preset);
    bool hasScreenEffects() const;
    int &strength(ScreenEffect effect) { return screenEffects[static_cast<std::size_t>(effect)]; }
    int strength(ScreenEffect effect) const { return screenEffects[static_cast<std::size_t>(effect)]; }
    bool operator==(const Settings &) const = default;
};

} // namespace PulsePointer
