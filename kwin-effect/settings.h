#pragma once

#include <QtGlobal>

namespace PulsePointer
{

enum class VisualizationMode {
    Elastic = 0,
    Bar = 1,
    Circle = 2,
    Spectrum = 3,
    Oscilloscope = 4,
    SpectrumHalo = 5,
    Wobble = 6,
};

struct Settings
{
    VisualizationMode mode = VisualizationMode::Elastic;
    int intensity = 100;
    bool beatRing = true;
    quint32 color = 0xCCFF4500;
    int overlaySize = 48;
    int responsiveness = 100;

    static Settings defaults();
    static Settings load();
    void save() const;

    bool operator==(const Settings &) const = default;
};

} // namespace PulsePointer
