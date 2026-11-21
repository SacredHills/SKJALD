#pragma once

#include "HalfbandStage.h"
#include "../Utilities/DelayLine.h"

#include <array>
#include <vector>

namespace skjald
{
/** High-accuracy inter-sample peak detector: a cascade of 2x halfband interpolators
    (2x, 4x or 8x). The first stage has a narrow transition band centred on fs/2
    (flat to ~0.485 fs), so full-band material such as noise, cymbals and clipped
    transients is measured correctly, which short single-stage kernels under-read.
    The later stages only need to protect that band. Cost is about 470 multiplies per
    input sample at 8x. Verified against independent long-kernel and FFT reconstructions
    (see Tests/ and scripts/verify_true_peak.py).

    For each input sample it outputs the maximum |y| of the F interpolated samples of
    that sample period, delayed by getLatency() samples (an integer). */
class TruePeakDetector
{
public:
    static constexpr int kMaxBlock = 256;

    /** edge: upper edge of the first stage's transition band relative to the input rate
        (0.515 = transition 0.485-0.515 fs). Wider is cheaper but under-reads content near
        the input Nyquist. */
    void prepare (int factor, double attenuationDb = 70.0, double edge = kDefaultEdge, bool refinePeaks = false);

    /** Default transition 0.495-0.505 fs: content closer to Nyquist than that is ambiguous
        for any finite reconstruction filter (see docs/TECHNICAL.md). */
    static constexpr double kDefaultEdge = 0.505;
    void reset() noexcept;

    /** in: n samples -> peaks: n values (any n; processed in chunks). */
    void process (const float* in, float* peaks, int n) noexcept;

    /** With refinePeaks, each local maximum of the oversampled stream is refined by parabolic
        interpolation through its two neighbours. Even at 8x a tone near 0.45 fs is sampled only
        every 0.35 rad, so the raw maximum can miss the crest by up to 0.13 dB; the parabola removes
        that error (what remains, < 0.01 dB, is the halfband stages' passband ripple). Adds one
        sample of latency (the next sample period is needed for the neighbour of the last
        oversampled point). */
    int getLatency() const noexcept { return latency; }
    int getFactor() const noexcept { return 1 << stages; }

private:
    int stages = 3, padTop = 0, latency = 0;
    bool refine = false;
    float prevPrevTop = 0.0f, prevTop = 0.0f, pendingPeak = 0.0f; // refinement state
    std::array<HalfbandStage, 3> filters;
    std::array<std::vector<float>, 4> work;
    DelayLine<float> pad;
};

} // namespace skjald
