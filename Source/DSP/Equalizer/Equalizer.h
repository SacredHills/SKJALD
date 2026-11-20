#pragma once

#include "EQBand.h"
#include "../Utilities/Smoothers.h"

#include <array>
#include <vector>

namespace skjald
{
/** Eight-band minimum-phase parametric mastering EQ (zero latency).
    Linked stereo parameters, independent per-channel filter state. */
class Equalizer
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const EQParams& p) noexcept;

    /** In-place processing of float channels. numSamples must be <= maxBlockSize. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Linear magnitude of the full EQ at a frequency, using the same designs as the audio path. */
    static double magnitudeAt (const EQParams& p, double frequencyHz, double sampleRate) noexcept;

private:
    std::array<EQBand, kNumEQBands> bands;
    std::array<std::vector<double>, kMaxChannels> scratch;
    std::array<std::vector<float>, kMaxChannels> dry;
    LinearSmoother wet; // 1 = EQ active, 0 = bypassed
    int maxBlock = 0;
    bool firstParams = true;
};

} // namespace skjald
