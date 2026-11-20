#pragma once

#include "EnvelopeBlocks.h"
#include "../Oversampling/TruePeakDetector.h"
#include <vector>
#include "../Utilities/DelayLine.h"

#include <array>

namespace skjald
{
/** Output verification stage at the base rate, after the oversampled limiter has been
    decimated. Decimation filters remove the harmonics created by limiting/clipping, which
    can create new (inter-sample) overs. The guard measures the decimated signal with an
    8x halfband-cascade detector (true-peak mode) or the raw samples (sample-peak mode) and applies a
    short (~1 ms) lookahead gain correction with the same min/average guarantee as the main
    core. It is fully stereo-linked so it cannot shift the image.

    Its latency is constant regardless of mode. */
class TruePeakGuard
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kDetectorFactor = 8;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** ceiling: per-sample linear target at the input time of each sample. */
    void process (float* const* io, int numChannels, int numSamples, const float* ceiling, bool truePeak) noexcept;

    int getLatency() const noexcept { return latency; }
    float getMinGain() const noexcept { return blockMinGain; }

private:
    int window = 48, latency = 0;
    float releaseCoeff = 0.99f, env = 1.0f;
    SlidingMin hold;
    std::array<BoxFilter, 2> boxes;
    struct Channel
    {
        TruePeakDetector detector;
        DelayLine<float> samplePeakAlign, signal;
        std::vector<float> peaks;
    };
    std::array<Channel, kMaxChannels> ch;
    float blockMinGain = 1.0f;
};

} // namespace skjald
