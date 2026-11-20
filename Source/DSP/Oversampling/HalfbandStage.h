#pragma once

#include <vector>
#include "../Utilities/DelayLine.h"

namespace skjald
{
/** One 2x linear-phase halfband FIR stage (Kaiser-windowed sinc), polyphase.
    Length N = 4K - 1, centre index c = 2K - 1. Every other tap is zero except the centre,
    so each output costs about N/2 multiplies.

    Latency: up + down round trip = c samples at the stage's lower rate. */
class HalfbandStage
{
public:
    /** transitionWidth is relative to the stage's output (higher) rate. */
    void design (double transitionWidth, double attenuationDb);
    void reset() noexcept;

    /** in: n samples at the low rate -> out: 2n samples at the high rate. */
    void upsample (const float* in, float* out, int n) noexcept;

    /** in: 2n samples at the high rate -> out: n samples at the low rate. */
    void downsample (const float* in, float* out, int n) noexcept;

    int getNumTaps() const noexcept { return numTaps; }
    int getCentre() const noexcept { return (numTaps - 1) / 2; }

private:
    int numTaps = 3, K = 1, M = 2;
    std::vector<float> taps;       // non-zero off-centre taps h[0], h[2], ... (M of them)
    std::vector<float> tapsUp;     // the same, times 2 (interpolation gain)

    std::vector<float> upHist;     // 2M, double-written
    int upPos = 0;
    std::vector<float> downHist;   // 2M, double-written (even-indexed input samples)
    int downPos = 0;
    DelayLine<float> oddDelay;     // odd-indexed input samples, delayed K pairs
};

} // namespace skjald
