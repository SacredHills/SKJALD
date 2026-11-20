#pragma once

#include <array>
#include <complex>
#include <cmath>

namespace skjald
{
/** Normalised biquad coefficients (a0 == 1). A first-order section has b2 == a2 == 0. */
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double omega) const noexcept
    {
        const std::complex<double> z1 = std::polar (1.0, -omega);
        const std::complex<double> z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    double magnitude (double omega) const noexcept { return std::abs (response (omega)); }

    bool isStable() const noexcept
    {
        // Jury criterion for a second-order denominator 1 + a1 z^-1 + a2 z^-2
        return std::abs (a2) < 1.0 && std::abs (a1) < 1.0 + a2;
    }

    bool operator== (const BiquadCoeffs& o) const noexcept
    {
        return b0 == o.b0 && b1 == o.b1 && b2 == o.b2 && a1 == o.a1 && a2 == o.a2;
    }
};

/** Direct Form I state in double precision. DF1 is used because it tolerates
    per-block coefficient updates (parameter smoothing) without internal-state
    discontinuities, and double precision keeps low-frequency sections at high
    sample rates numerically clean. */
struct BiquadState
{
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

    inline double process (const BiquadCoeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

constexpr int kMaxSectionsPerBand = 4; // up to 8th order (48 dB/oct)

struct SectionCascade
{
    std::array<BiquadCoeffs, kMaxSectionsPerBand> sections {};
    int numSections = 0;

    std::complex<double> response (double omega) const noexcept
    {
        std::complex<double> h (1.0, 0.0);
        for (int i = 0; i < numSections; ++i)
            h *= sections[(size_t) i].response (omega);
        return h;
    }

    double magnitude (double omega) const noexcept { return std::abs (response (omega)); }
};

} // namespace skjald
