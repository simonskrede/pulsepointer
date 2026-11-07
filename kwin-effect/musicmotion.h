#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace PulsePointer
{

// Audio envelopes and event ages are independent of paint rate and monitor count.
struct MusicMotion {
    std::array<float, 4> target{};
    std::array<float, 4> bands{};
    std::array<float, 4> waveAge{10, 10, 10, 10};
    std::array<float, 4> waveStrength{};
    float pulse = 0;
    float lastOnset = 0;
    float quietSeconds = 0;
    float dropAge = 10;
    float dropStrength = 0;
    float time = 0;
    float hue = 0;
    bool played = false;
    std::size_t nextWave = 0;

    void audio(float level, float bass, float mid, float treble, float onset)
    {
        if (quietSeconds > 0.75f && level > 0.18f && level > target[0] + 0.12f) {
            dropAge = 0;
            dropStrength = std::min(1.0f, level * 2);
        }
        target = {level, bass, mid, treble};
        if (level > 0.04f) {
            played = true;
        }
        if (onset >= 0.12f && lastOnset < 0.12f) {
            waveAge[nextWave] = 0;
            waveStrength[nextWave] = onset;
            nextWave = (nextWave + 1) % waveAge.size();
            pulse = std::max(pulse, onset);
        }
        lastOnset = onset;
    }

    void advance(float seconds, int attackMs, int releaseMs)
    {
        seconds = std::clamp(seconds, 0.0f, 0.1f);
        for (std::size_t i = 0; i < bands.size(); ++i) {
            const float duration = (target[i] > bands[i] ? attackMs : releaseMs) / 1000.0f;
            bands[i] += (target[i] - bands[i]) * (1 - std::exp(-seconds / duration));
        }
        pulse *= std::exp(-seconds / (releaseMs / 1000.0f));
        quietSeconds = target[0] < 0.025f ? quietSeconds + seconds : 0;
        for (float &age : waveAge) {
            age = std::min(10.0f, age + seconds);
        }
        dropAge = std::min(10.0f, dropAge + seconds);
        time = std::fmod(time + seconds * (0.2f + 0.8f * activity()), 3600.0f);
        hue = std::fmod(hue + seconds * (0.025f + 0.06f * bands[0]) * (1 - 2 * pulse) + 1, 1.0f);
    }

    float activity() const
    {
        return std::clamp(3 * *std::max_element(bands.begin(), bands.end()) + pulse, 0.0f, 1.0f);
    }

    float collapse() const
    {
        if (!played) {
            return 0;
        }
        return std::clamp((quietSeconds - 0.75f) / 2.0f, 0.0f, 1.0f)
            * std::clamp((8 - quietSeconds) / 3.0f, 0.0f, 1.0f);
    }

    bool moving() const
    {
        return activity() > 0.002f || target[0] > 0.002f || target[1] > 0.002f
            || target[2] > 0.002f || target[3] > 0.002f || collapse() > 0
            || (played && quietSeconds < 8)
            || dropAge < 2;
    }
};

} // namespace PulsePointer
