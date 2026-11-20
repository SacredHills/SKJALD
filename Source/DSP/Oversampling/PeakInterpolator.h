#pragma once

#include <vector>

namespace skjald
{
/** Polyphase windowed-sinc interpolator used for inter-sample (true) peak detection.
    It does not reconstruct an output stream; for every input sample it returns the
    largest absolute value among the F interpolated points of that sample period.

    Kernel length 2*D*F + 1 with zero crossings at multiples of F, so phase 0 is the input
    delayed by exactly D samples and the detection latency is an integer (D). */
class PeakInterpolator
{
public:
    void prepare (int factor, int halfLengthInputSamples = 16, double attenuationDb = 70.0);
    void reset() noexcept;

    /** Push one input sample, return max |y| over the F interpolated phases (delayed by D). */
    float processPeak (float x) noexcept;

    int getLatency() const noexcept { return D; }
    int getFactor() const noexcept { return F; }

private:
    int F = 1, D = 0, J = 1; // J taps per phase
    std::vector<float> coeffs; // [phase][j], F * J
    std::vector<float> hist;   // 2J, double-written
    int pos = 0;
};

} // namespace skjald
