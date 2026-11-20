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
    The later stages only need to protect that band. Cost is about 340 multiplies per
    input sample at 8x. Verified against an independent long-kernel reconstruction to
    within 0.01 dB on white noise and sines up to 0.47 fs.

    For each input sample it outputs the maximum |y| of the F interpolated samples of
    that sample period, delayed by getLatency() samples (an integer). */
class TruePeakDetector
{
public:
    static constexpr int kMaxBlock = 256;

    /** edge: upper edge of the first stage's transition band relative to the input rate
        (0.515 = transition 0.485-0.515 fs). Wider is cheaper but under-reads content near
        the input Nyquist. */
    void prepare (int factor, double attenuationDb = 70.0, double edge = 0.515);
    void reset() noexcept;

    /** in: n samples -> peaks: n values (any n; processed in chunks). */
    void process (const float* in, float* peaks, int n) noexcept;

    int getLatency() const noexcept { return latency; }
    int getFactor() const noexcept { return 1 << stages; }

private:
    int stages = 3, padTop = 0, latency = 0;
    std::array<HalfbandStage, 3> filters;
    std::array<std::vector<float>, 4> work;
    DelayLine<float> pad;
};

} // namespace skjald
