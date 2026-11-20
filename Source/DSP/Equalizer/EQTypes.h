#pragma once

#include <array>
#include <cstddef>

namespace skjald
{
enum class FilterType : int
{
    Bell = 0,
    LowShelf,
    HighShelf,
    HighPass,
    LowPass,
    Notch
};

constexpr int kNumFilterTypes = 6;
constexpr int kNumEQBands = 8;

/** Slope choices in dB/octave (index used by the parameter system). */
constexpr std::array<int, 6> kSlopeChoicesDbPerOct { 6, 12, 18, 24, 36, 48 };

inline int slopeIndexToOrder (int slopeIndex) noexcept
{
    if (slopeIndex < 0) slopeIndex = 0;
    if (slopeIndex > 5) slopeIndex = 5;
    return kSlopeChoicesDbPerOct[(std::size_t) slopeIndex] / 6;
}

inline bool filterTypeUsesGain (FilterType t) noexcept
{
    return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf;
}

inline bool filterTypeUsesSlope (FilterType t) noexcept
{
    return t == FilterType::LowShelf || t == FilterType::HighShelf || t == FilterType::HighPass || t == FilterType::LowPass;
}

struct EQBandParams
{
    bool enabled = false;
    FilterType type = FilterType::Bell;
    float frequency = 1000.0f; // Hz, 20 .. 20000
    float gainDb = 0.0f;       // -24 .. +24
    float q = 0.707f;          // 0.1 .. 10
    int slopeIndex = 1;        // index into kSlopeChoicesDbPerOct

    bool sameStructure (const EQBandParams& o) const noexcept
    {
        return enabled == o.enabled && type == o.type && (! filterTypeUsesSlope (type) || slopeIndex == o.slopeIndex);
    }
};

struct EQParams
{
    std::array<EQBandParams, kNumEQBands> bands {};
    bool bypass = false;
};

} // namespace skjald
