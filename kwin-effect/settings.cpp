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
    result.enabled = group.readEntry("Enabled", fallback.enabled);
    if (group.hasKey("CursorAnimation")) {
        result.cursor = static_cast<CursorAnimation>(std::clamp(
            group.readEntry("CursorAnimation", int(fallback.cursor)), 0, 2));
        result.pointerVisualizations = group.readEntry("PointerVisualizations", quint32(0))
            & pointerVisualizationMask;
    } else {
        const int legacy = std::clamp(group.readEntry("Mode", 0), 0, 6);
        result.cursor = legacy == 0 ? CursorAnimation::Elastic
            : legacy == 6 ? CursorAnimation::Wobble : CursorAnimation::None;
        result.pointerVisualizations = pointerBit(static_cast<VisualizationMode>(legacy));
    }
    result.intensity = std::clamp(group.readEntry("Intensity", fallback.intensity), 0, 200);
    result.beatRing = group.readEntry("BeatRing", fallback.beatRing);
    if (!group.hasKey("CursorAnimation") && result.cursor == CursorAnimation::None) {
        // The old overlay modes never displayed the ring, even when BeatRing was true.
        result.beatRing = false;
    }
    result.hotspotLocator = group.readEntry("HotspotLocator", fallback.hotspotLocator);
    result.color = group.readEntry("Color", fallback.color);
    result.overlaySize = std::clamp(group.readEntry("OverlaySize", fallback.overlaySize), 16, 128);
    result.responsiveness = std::clamp(
        group.readEntry("Responsiveness", fallback.responsiveness), 50, 200);
    for (std::size_t i = 0; i < screenEffectCount; ++i) {
        result.screenEffects[i] = std::clamp(group.readEntry(screenEffectCatalog[i].key, 0), 0, 100);
    }
    result.screenIntensity = std::clamp(group.readEntry("ScreenIntensity", 100), 0, 200);
    result.edgeReach = std::clamp(group.readEntry("EdgeReach", 100), 16, 400);
    result.edgeDensity = std::clamp(group.readEntry("EdgeDensity", 16), 4, 64);
    result.edgeSoftness = std::clamp(group.readEntry("EdgeSoftness", 65), 0, 100);
    result.edgeProfile = std::clamp(group.readEntry("EdgeProfile", 0), 0, 4);
    result.attackMs = std::clamp(group.readEntry("AttackMs", 45), 10, 500);
    result.releaseMs = std::clamp(group.readEntry("ReleaseMs", 350), 50, 2000);
    result.monitors = static_cast<MonitorScope>(std::clamp(group.readEntry("Monitors", 0), 0, 1));
    return result;
}

void Settings::save() const
{
    KConfigGroup group(KSharedConfig::openConfig(QString::fromLatin1(s_configFile)),
                       QString::fromLatin1(s_configGroup));
    group.writeEntry("Enabled", enabled);
    group.writeEntry("CursorAnimation", static_cast<int>(cursor));
    group.writeEntry("PointerVisualizations", pointerVisualizations);
    group.writeEntry("Intensity", intensity);
    group.writeEntry("BeatRing", beatRing);
    group.writeEntry("HotspotLocator", hotspotLocator);
    group.writeEntry("Color", color);
    group.writeEntry("OverlaySize", overlaySize);
    group.writeEntry("Responsiveness", responsiveness);
    for (std::size_t i = 0; i < screenEffectCount; ++i) {
        group.writeEntry(screenEffectCatalog[i].key, screenEffects[i]);
    }
    group.writeEntry("ScreenIntensity", screenIntensity);
    group.writeEntry("EdgeReach", edgeReach);
    group.writeEntry("EdgeDensity", edgeDensity);
    group.writeEntry("EdgeSoftness", edgeSoftness);
    group.writeEntry("EdgeProfile", edgeProfile);
    group.writeEntry("AttackMs", attackMs);
    group.writeEntry("ReleaseMs", releaseMs);
    group.writeEntry("Monitors", static_cast<int>(monitors));
    group.sync();
}

bool Settings::hasScreenEffects() const
{
    return screenIntensity > 0 && std::any_of(screenEffects.begin(), screenEffects.end(),
                                            [](int value) { return value > 0; });
}

void Settings::applyPreset(Preset preset)
{
    screenEffects.fill(0);
    pointerVisualizations = 0;
    cursor = CursorAnimation::Elastic;
    screenIntensity = 100;
    edgeReach = 100;
    edgeDensity = 16;
    edgeSoftness = 65;
    edgeProfile = 0;
    attackMs = 45;
    releaseMs = 350;
    switch (preset) {
    case Preset::Velvet:
        strength(ScreenEffect::BreathingFrame) = 35;
        strength(ScreenEffect::ChromaBreathing) = 25;
        releaseMs = 550;
        break;
    case Preset::Voltage:
        cursor = CursorAnimation::Wobble;
        strength(ScreenEffect::RazorFrame) = 65;
        strength(ScreenEffect::Sparks) = 70;
        strength(ScreenEffect::Prism) = 40;
        edgeSoftness = 10;
        edgeDensity = 32;
        releaseMs = 180;
        break;
    case Preset::Underwater:
        strength(ScreenEffect::LiquidBorder) = 55;
        strength(ScreenEffect::HueTide) = 20;
        strength(ScreenEffect::Refraction) = 45;
        releaseMs = 800;
        break;
    case Preset::Aurora:
        strength(ScreenEffect::Aurora) = 65;
        strength(ScreenEffect::Particles) = 30;
        strength(ScreenEffect::SpectralPalette) = 20;
        releaseMs = 1000;
        break;
    case Preset::Meltdown:
        cursor = CursorAnimation::Wobble;
        strength(ScreenEffect::RazorFrame) = 70;
        strength(ScreenEffect::Gravity) = 60;
        strength(ScreenEffect::Feedback) = 50;
        strength(ScreenEffect::Drop) = 65;
        edgeReach = 180;
        edgeSoftness = 15;
        break;
    }
}

} // namespace PulsePointer
