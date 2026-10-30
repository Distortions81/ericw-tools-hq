#pragma once

#include <common/qvec.hh>
#include <cmath>
#include <cstdint>

// Stateless sample sequences: reproducible across worker counts and compile runs.
inline uint32_t LightSampleHash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

inline float LightSampleUnit(uint32_t seed)
{
    return ((LightSampleHash(seed) >> 9) + 0.5f) / 8388608.0f;
}

inline qvec2f StratifiedLightmapOffset(int s, int t, int factor, uint32_t seed)
{
    const uint32_t key = seed ^ (static_cast<uint32_t>(s) * 0x9e3779b9u) ^
        (static_cast<uint32_t>(t) * 0x85ebca6bu);
    return {(s + LightSampleUnit(key)) / factor - 0.5f,
        (t + LightSampleUnit(key ^ 0xa511e9b3u)) / factor - 0.5f};
}

inline double LightRadicalInverse(uint32_t value, uint32_t base)
{
    double result = 0, place = 1.0 / base;
    while (value) {
        result += (value % base) * place;
        value /= base;
        place /= base;
    }
    return result;
}

inline qvec3f PairedAreaLightOffset(int index, int count, uint32_t seed)
{
    // Keep the original source at its center and place the others in antipodal pairs.
    // An even count has one extra center sample. Equal weights keep the centroid fixed.
    if (index == 0 || (count % 2 == 0 && index == count - 1))
        return qvec3f{0};
    const uint32_t pair = (index - 1) / 2;
    const int pairs = (count - 1) / 2;
    const double z = 1 - 2 * ((pair + 0.5) / pairs);
    const double phi = 6.283185307179586 * (LightRadicalInverse(pair + 1, 2) + LightSampleUnit(seed));
    const double radius = std::cbrt(LightRadicalInverse(pair + 1, 3));
    const double xy = std::sqrt(std::max(0.0, 1 - z * z));
    const float sign = index % 2 ? 1.0f : -1.0f;
    return qvec3f{static_cast<float>(radius * xy * std::cos(phi)),
        static_cast<float>(radius * xy * std::sin(phi)), static_cast<float>(radius * z)} * sign;
}
