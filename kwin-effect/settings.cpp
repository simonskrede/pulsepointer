#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QString>

#include <algorithm>

namespace
{

constexpr auto s_configFile = "kwinrc";
constexpr auto s_configGroup = "Effect-pulsepointer";

}

namespace PulsePointer
{

Settings Settings::defaults()
{
    return {};
}

Settings Settings::load()
{
    const Settings fallback = defaults();
    const KConfigGroup group(KSharedConfig::openConfig(QString::fromLatin1(s_configFile)),
                             QString::fromLatin1(s_configGroup));
    Settings result;
    const int mode = group.readEntry("Mode", static_cast<int>(fallback.mode));
    result.mode = static_cast<VisualizationMode>(std::clamp(
        mode,
        static_cast<int>(VisualizationMode::Elastic),
        static_cast<int>(VisualizationMode::Wobble)));
    result.intensity = std::clamp(group.readEntry("Intensity", fallback.intensity), 0, 200);
    result.beatRing = group.readEntry("BeatRing", fallback.beatRing);
    result.color = group.readEntry("Color", fallback.color);
    result.overlaySize = std::clamp(group.readEntry("OverlaySize", fallback.overlaySize), 16, 128);
    result.responsiveness = std::clamp(
        group.readEntry("Responsiveness", fallback.responsiveness), 50, 200);
    return result;
}

void Settings::save() const
{
    KConfigGroup group(KSharedConfig::openConfig(QString::fromLatin1(s_configFile)),
                       QString::fromLatin1(s_configGroup));
    group.writeEntry("Mode", static_cast<int>(mode));
    group.writeEntry("Intensity", intensity);
    group.writeEntry("BeatRing", beatRing);
    group.writeEntry("Color", color);
    group.writeEntry("OverlaySize", overlaySize);
    group.writeEntry("Responsiveness", responsiveness);
    group.sync();
}

} // namespace PulsePointer
