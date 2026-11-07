#pragma once

#include <array>
#include <cstddef>

namespace PulsePointer
{

// Order is also the uniform-array order in screen.frag. Keys are stable config names.
enum class ScreenEffect : std::size_t {
    BreathingFrame, RazorFrame, LiquidBorder, PerimeterWave, CornerWaves, Sparks,
    ChromaBreathing, BassBloom, SpectralPalette, HueTide, SelectiveColor, ColorEchoes,
    Gravity, Refraction, Prism, Aurora, Terrain, Particles, Feedback, Drop,
    Count,
};
constexpr std::size_t screenEffectCount = static_cast<std::size_t>(ScreenEffect::Count);

struct EffectDescription {
    const char *key;
    const char *name;
    const char *description;
};

inline constexpr std::array<EffectDescription, screenEffectCount> screenEffectCatalog{{
    {"BreathingFrame", "Breathing frame", "A soft edge glow expands with the bass and fades between notes."},
    {"RazorFrame", "Razor frame", "Spectrum-driven teeth kick inward on sharp musical attacks."},
    {"LiquidBorder", "Liquid border", "Broad bass waves and fine treble ripples flow along the edges."},
    {"PerimeterWave", "Perimeter oscilloscope", "The live waveform wraps around the screen with traveling ripples."},
    {"CornerWaves", "Corner shockwaves", "Musical attacks launch expanding arcs from all four corners."},
    {"Sparks", "Chasing sparks", "Bright sparks race around the frame, leaving fading trails."},
    {"ChromaBreathing", "Chroma breathing", "Desktop colors become muted between notes and bloom into saturation."},
    {"BassBloom", "Bass bloom", "Warm light gathers near the edges on low notes."},
    {"SpectralPalette", "Spectral palette", "Bass, mids, and treble mix warm, green, and blue light."},
    {"HueTide", "Hue tide", "Desktop hues rotate slowly; strong attacks briefly reverse the current."},
    {"SelectiveColor", "Selective color", "One moving family of hues stays vivid while other colors become muted."},
    {"ColorEchoes", "Color echoes", "Colored outlines peel away from desktop edges after strong attacks."},
    {"Gravity", "Gravity well", "Bass bends the desktop toward the pointer, followed by an elastic rebound."},
    {"Refraction", "Refractive ripples", "Expanding glass-like waves follow the pointer and gently displace the desktop."},
    {"Prism", "Prism kick", "Red and blue channels separate briefly on musical attacks."},
    {"Aurora", "Aurora curtains", "Translucent ribbons sway from the top edge, with fine treble detail."},
    {"Terrain", "Spectral terrain", "A mountain range grows from the bottom edge following the spectrum."},
    {"Particles", "Particle weather", "Bass launches particles, mids steer them, and treble makes them glitter."},
    {"Feedback", "Feedback ghosts", "Faint expanding copies of the desktop linger after strong attacks."},
    {"Drop", "Drop collapse", "Quiet passages pull light toward the corners; returning energy releases it."},
}};

} // namespace PulsePointer
