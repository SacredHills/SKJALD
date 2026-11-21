#pragma once

#include "EnvelopeBlocks.h"
#include "../Oversampling/TruePeakDetector.h"
#include "../Oversampling/PeakInterpolator.h"
#include "../Oversampling/ItuPeakDetector.h"
#include "../Filters/LinearPhaseLowpass.h"
#include <vector>
#include "../Utilities/DelayLine.h"

#include <array>

namespace skjald
{
/** Output verification stage at the base rate, after the oversampled limiter has been
    decimated. Decimation filters remove the harmonics created by limiting/clipping, which
    can create new (inter-sample) overs, and fold part of them into the band just below
    Nyquist.

    1. Band-limit: a linear-phase low-pass (flat to 0.4535 fs, i.e. 20 kHz at 44.1 kHz, the
       same passband as the oversampler; >= 100 dB from 0.4875 fs; designed for 105 dB) removes the content that
       no finite true-peak detector can measure accurately.
    2. Detection (true-peak mode): the larger of an 8x halfband-cascade detector (accurate to
       0.01 dB up to 0.49 fs), a short interpolator that over-reads near Nyquist, the ITU-R
       BS.1770-4 Annex 2 reference meter and the sample peak. Sample-peak mode uses the samples.
    3. Gain: ~1 ms lookahead correction with the min/average guarantee of the main core, but
       with the minimum taken over +-kNeighbourhood samples around every sample, so the gain is
       at or below the required reduction on every sample that contributes to an inter-sample
       peak, not only at the peak sample (the guard's own gain ramp cannot create overs).

    Fully stereo-linked, so it cannot shift the image. Its latency is constant in every mode. */
class TruePeakGuard
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kDetectorFactor = 8;
    static constexpr int kNyquistHalfLength = 16;
    static constexpr double kNyquistCutoff = 1.12;
    static constexpr int kNeighbourhood = 8;
    static constexpr double kBandPassEdge = 0.4535, kBandStopEdge = 0.4875, kBandAttenuationDb = 105.0;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** ceiling: per-sample linear target at the input time of each sample. */
    void process (float* const* io, int numChannels, int numSamples, const float* ceiling, bool truePeak) noexcept;

    int getLatency() const noexcept { return latency; }
    float getMinGain() const noexcept { return blockMinGain; }

private:
    int window = 48, latency = 0, detLatency = 0, bandLatency = 0;
    float releaseCoeff = 0.99f, env = 1.0f;
    SlidingMin hold;
    std::array<BoxFilter, 2> boxes;
    struct Channel
    {
        TruePeakDetector detector;
        PeakInterpolator nyquistGuard; // over-reads content near fs/2 (safe side)
        LinearPhaseLowpass bandLimit;
        ItuPeakDetector itu;
        DelayLine<float> samplePeakAlign, nyquistAlign, ituAlign, signal;
        std::vector<float> limited;
        std::vector<float> peaks;
    };
    std::array<Channel, kMaxChannels> ch;
    DelayLine<float> ceilingAlign; // ceiling stream aligned with the detector readings
    bool ceilingPrimed = false;
    float blockMinGain = 1.0f;
};

} // namespace skjald
