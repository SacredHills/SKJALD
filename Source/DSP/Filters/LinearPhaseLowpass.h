#pragma once

#include <vector>

namespace skjald
{
/** Odd-length linear-phase low-pass FIR (Kaiser-windowed sinc) with folded symmetric taps.
    Used as the output band-limiter in front of the true-peak guard: content in the last few
    per cent below Nyquist cannot be measured accurately by any finite true-peak detector, so
    it is removed before the guard measures the signal.

    Frequencies are relative to the sample rate (0.5 = Nyquist). The length follows the Kaiser
    formula for the requested transition band and attenuation. Latency: (N - 1) / 2 samples. */
class LinearPhaseLowpass
{
public:
    void design (double passEdge, double stopEdge, double attenuationDb);
    void reset() noexcept;

    float process (float x) noexcept;

    int getLatency() const noexcept { return half; }
    int getNumTaps() const noexcept { return 2 * half + 1; }

    /** Magnitude response at frequency f (relative to the sample rate), for tests. */
    double magnitudeAt (double f) const noexcept;

private:
    int half = 0;              // N = 2 * half + 1
    std::vector<float> taps;   // taps[0] = centre, taps[k] = h[centre +- k]
    std::vector<float> hist;   // 2N, double-written
    int pos = 0, N = 1;
};

} // namespace skjald
