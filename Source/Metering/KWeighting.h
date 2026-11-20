#pragma once

#include "../DSP/Filters/Biquad.h"
#include "../DSP/Utilities/MathUtils.h"

#include <cmath>

namespace skjald
{
/** ITU-R BS.1770 K-weighting (pre-filter high shelf + RLB high-pass), with coefficients
    derived for any sample rate from the analog parameters of the 48 kHz reference
    (same derivation as used by libebur128). At 48 kHz they equal the published values. */
struct KWeighting
{
    BiquadCoeffs shelf, highpass;

    static KWeighting design (double fs) noexcept
    {
        KWeighting k;
        {
            const double f0 = 1681.974450955533;
            const double G = 3.999843853973347;
            const double Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            k.shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            k.shelf.b1 = 2.0 * (K * K - Vh) / a0;
            k.shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            k.shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            k.shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444;
            const double Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            k.highpass.b0 = 1.0;
            k.highpass.b1 = -2.0;
            k.highpass.b2 = 1.0;
            k.highpass.a1 = 2.0 * (K * K - 1.0) / a0;
            k.highpass.a2 = (1.0 - K / Q + K * K) / a0;
        }
        return k;
    }
};

struct KWeightingFilter
{
    KWeighting coeffs;
    BiquadState s1, s2;

    void prepare (double fs) noexcept { coeffs = KWeighting::design (fs); reset(); }
    void reset() noexcept { s1.reset(); s2.reset(); }
    inline double process (double x) noexcept { return s2.process (coeffs.highpass, s1.process (coeffs.shelf, x)); }
};

} // namespace skjald
