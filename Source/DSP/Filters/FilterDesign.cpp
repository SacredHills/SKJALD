#include "FilterDesign.h"
#include "../Utilities/MathUtils.h"

#include <cmath>
#include <algorithm>

namespace skjald::FilterDesign
{
double AnalogSection::magnitudeSquared (double w) const noexcept
{
    const double w2 = w * w;
    const double nr = n0 - n2 * w2, ni = n1 * w;
    const double dr = d0 - d2 * w2, di = d1 * w;
    const double den = dr * dr + di * di;
    return den > 0.0 ? (nr * nr + ni * ni) / den : 0.0;
}

BiquadCoeffs makeNumeratorMinimumPhase (const BiquadCoeffs& c) noexcept
{
    BiquadCoeffs r = c;
    if (c.b0 == 0.0)
        return r;

    if (c.b2 == 0.0)
    {
        // first order numerator b0 + b1 z^-1 : zero at -b1/b0
        if (std::abs (c.b1) > std::abs (c.b0))
            std::swap (r.b0, r.b1);
        return r;
    }

    const double disc = c.b1 * c.b1 - 4.0 * c.b0 * c.b2;
    if (disc < 0.0)
    {
        // complex pair, |z|^2 = b2 / b0
        if (c.b2 / c.b0 > 1.0)
            std::swap (r.b0, r.b2);
        return r;
    }

    const double sq = std::sqrt (disc);
    double r1 = (-c.b1 + sq) / (2.0 * c.b0);
    double r2 = (-c.b1 - sq) / (2.0 * c.b0);
    double gain = c.b0;
    if (std::abs (r1) > 1.0) { gain *= std::abs (r1); r1 = 1.0 / r1; }
    if (std::abs (r2) > 1.0) { gain *= std::abs (r2); r2 = 1.0 / r2; }
    // Keep the sign of the response at DC unchanged
    const double dcOld = c.b0 + c.b1 + c.b2;
    double nb0 = gain, nb1 = -gain * (r1 + r2), nb2 = gain * r1 * r2;
    const double dcNew = nb0 + nb1 + nb2;
    if (dcOld * dcNew < 0.0) { nb0 = -nb0; nb1 = -nb1; nb2 = -nb2; }
    r.b0 = nb0; r.b1 = nb1; r.b2 = nb2;
    return r;
}

BiquadCoeffs invert (const BiquadCoeffs& c) noexcept
{
    BiquadCoeffs r;
    if (std::abs (c.b0) < 1.0e-30)
        return r;
    const double inv = 1.0 / c.b0;
    r.b0 = inv;
    r.b1 = c.a1 * inv;
    r.b2 = c.a2 * inv;
    r.a1 = c.b1 * inv;
    r.a2 = c.b2 * inv;
    return r;
}

BiquadCoeffs matchedDesign (const AnalogSection& a, double wm) noexcept
{
    BiquadCoeffs c;
    const double H0 = a.magnitudeSquared (0.0);
    const double Hpi = a.magnitudeSquared (kPi);

    if (! a.isSecondOrder())
    {
        // First order: impulse-invariant pole, numerator matched at DC and Nyquist.
        const double sp = a.d0 / a.d1; // pole at s = -sp
        const double p = std::exp (-sp);
        c.a1 = -p;
        c.a2 = 0.0;
        const double s0 = std::sqrt (H0) * (1.0 - p);
        const double spi = std::sqrt (Hpi) * (1.0 + p);
        c.b0 = 0.5 * (s0 + spi);
        c.b1 = 0.5 * (s0 - spi);
        c.b2 = 0.0;
        return makeNumeratorMinimumPhase (c);
    }

    const double wp = std::sqrt (a.d0 / a.d2);
    const double zeta = (a.d1 / a.d2) / (2.0 * wp);
    const double e = std::exp (-zeta * wp);
    if (zeta <= 1.0)
        c.a1 = -2.0 * e * std::cos (wp * std::sqrt (1.0 - zeta * zeta));
    else
        c.a1 = -2.0 * e * std::cosh (wp * std::sqrt (zeta * zeta - 1.0));
    c.a2 = e * e;

    const double A0 = (1.0 + c.a1 + c.a2) * (1.0 + c.a1 + c.a2);
    const double A1 = (1.0 - c.a1 + c.a2) * (1.0 - c.a1 + c.a2);
    const double A2 = -4.0 * c.a2;

    const double s = std::sin (0.5 * wm);
    const double phi1 = s * s;
    const double phi0 = 1.0 - phi1;
    const double phi2 = 4.0 * phi0 * phi1;

    const double Hw = a.magnitudeSquared (wm);
    const double B0 = H0 * A0;
    const double B1 = Hpi * A1;
    const double Aw = A0 * phi0 + A1 * phi1 + A2 * phi2;
    const double B2 = phi2 > 0.0 ? (Hw * Aw - B0 * phi0 - B1 * phi1) / phi2 : 0.0;

    const double sqB0 = std::sqrt (std::max (0.0, B0));
    const double sqB1 = std::sqrt (std::max (0.0, B1));
    const double W = 0.5 * (sqB0 + sqB1);
    c.b1 = 0.5 * (sqB0 - sqB1);
    const double disc = std::max (0.0, W * W + B2);
    c.b0 = 0.5 * (W + std::sqrt (disc));
    c.b2 = c.b0 != 0.0 ? -B2 / (4.0 * c.b0) : 0.0;
    return makeNumeratorMinimumPhase (c);
}

namespace
{
    constexpr double kSqrtHalf = 0.70710678118654752;

    double butterworthQ (int order, int k) noexcept // k = 1 .. order/2, ascending Q
    {
        // Pole angles from the negative real axis: (2k-1)pi/2n for even n, k*pi/n for odd n
        const double angle = (2.0 * k - 1.0 + (order % 2)) * kPi / (2.0 * order);
        return 1.0 / (2.0 * std::cos (angle));
    }

    AnalogSection lowpass2 (double w0, double q)
    {
        AnalogSection s;
        s.n0 = 1.0; s.n1 = 0.0; s.n2 = 0.0;
        s.d2 = 1.0 / (w0 * w0); s.d1 = 1.0 / (q * w0); s.d0 = 1.0;
        return s;
    }

    AnalogSection highpass2 (double w0, double q)
    {
        AnalogSection s = lowpass2 (w0, q);
        s.n0 = 0.0; s.n2 = 1.0 / (w0 * w0);
        return s;
    }

    AnalogSection lowpass1 (double w0)
    {
        AnalogSection s;
        s.n0 = w0; s.n1 = 0.0; s.n2 = 0.0;
        s.d0 = w0; s.d1 = 1.0; s.d2 = 0.0;
        return s;
    }

    AnalogSection highpass1 (double w0)
    {
        AnalogSection s = lowpass1 (w0);
        s.n0 = 0.0; s.n1 = 1.0;
        return s;
    }

    AnalogSection bell (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        AnalogSection s;
        s.n2 = 1.0 / (w0 * w0); s.n1 = A / (q * w0); s.n0 = 1.0;
        s.d2 = 1.0 / (w0 * w0); s.d1 = 1.0 / (A * q * w0); s.d0 = 1.0;
        return s;
    }

    // Second-order shelves (RBJ analog prototypes). gainDb must be >= 0 for low shelves
    // and <= 0 for high shelves so the poles lie at or below w0 (see designShelf).
    AnalogSection lowShelf2 (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sA = std::sqrt (A);
        AnalogSection s;
        s.n2 = A / (w0 * w0); s.n1 = A * sA / (q * w0); s.n0 = A * A;
        s.d2 = A / (w0 * w0); s.d1 = sA / (q * w0); s.d0 = 1.0;
        return s;
    }

    AnalogSection highShelf2 (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sA = std::sqrt (A);
        AnalogSection s;
        s.n2 = A * A / (w0 * w0); s.n1 = A * sA / (q * w0); s.n0 = A;
        s.d2 = 1.0 / (w0 * w0); s.d1 = sA / (q * w0); s.d0 = A;
        return s;
    }

    AnalogSection lowShelf1 (double w0, double gainDb)
    {
        const double sg = std::pow (10.0, gainDb / 40.0); // sqrt of linear gain
        AnalogSection s;
        s.n1 = 1.0; s.n0 = w0 * sg; s.n2 = 0.0;
        s.d1 = 1.0; s.d0 = w0 / sg; s.d2 = 0.0;
        return s;
    }

    AnalogSection highShelf1 (double w0, double gainDb)
    {
        const double g = std::pow (10.0, gainDb / 20.0);
        const double sg = std::sqrt (g);
        AnalogSection s;
        s.n1 = g; s.n0 = w0 * sg; s.n2 = 0.0;
        s.d1 = 1.0; s.d0 = w0 * sg; s.d2 = 0.0;
        return s;
    }

    /** Designs a shelf so that its poles never move above w0 (where impulse invariance would
        alias). Low-shelf cuts and high-shelf boosts are designed as the inverse of the mirrored
        shelf, which is exact in the analog domain: LS(-g) = 1 / LS(g), HS(g) = 1 / HS(-g). */
    BiquadCoeffs designShelf (bool low, int order, double w0, double gainDb, double q)
    {
        const bool needsInversion = low ? (gainDb < 0.0) : (gainDb > 0.0);
        const double g = needsInversion ? -gainDb : gainDb;
        AnalogSection a;
        if (order == 1)
            a = low ? lowShelf1 (w0, g) : highShelf1 (w0, g);
        else
            a = low ? lowShelf2 (w0, g, q) : highShelf2 (w0, g, q);
        BiquadCoeffs c = matchedDesign (a, w0);
        return needsInversion ? invert (c) : c;
    }

    BiquadCoeffs designNotch (double w0, double q)
    {
        const AnalogSection a = lowpass2 (w0, q);
        BiquadCoeffs c = matchedDesign (a, w0); // only the poles are used
        const double cw = std::cos (w0);
        const double k = (1.0 + c.a1 + c.a2) / (2.0 - 2.0 * cw);
        c.b0 = k;
        c.b1 = -2.0 * cw * k;
        c.b2 = k;
        return c;
    }
} // namespace

SectionCascade designBand (const EQBandParams& p, double sampleRate) noexcept
{
    SectionCascade out;
    if (! p.enabled || sampleRate <= 0.0)
        return out;

    const double f = std::clamp ((double) p.frequency, 10.0, kMaxRelativeFrequency * sampleRate);
    const double w0 = kTwoPi * f / sampleRate;
    const double q = std::clamp ((double) p.q, 0.05, 20.0);
    const double gainDb = std::clamp ((double) p.gainDb, -30.0, 30.0);
    const int order = slopeIndexToOrder (p.slopeIndex);

    auto add = [&out] (const BiquadCoeffs& c)
    {
        if (out.numSections < kMaxSectionsPerBand)
            out.sections[(size_t) out.numSections++] = c;
    };

    switch (p.type)
    {
        case FilterType::Bell:
            // Designed even at 0 dB (identity) so the section count never changes while the
            // gain is automated through zero; that keeps the filter state continuous.
            // Cuts are designed as the exact inverse of the mirrored boost (the analog bell
            // satisfies H_-g(s) = 1 / H_g(s)); a direct magnitude fit cannot place the deep
            // numerator notch near Nyquist, the inverted boost can.
            if (gainDb < 0.0)
                add (invert (matchedDesign (bell (w0, -gainDb, q), w0)));
            else
                add (matchedDesign (bell (w0, gainDb, q), w0));
            break;

        case FilterType::Notch:
            add (designNotch (w0, q));
            break;

        case FilterType::HighPass:
        case FilterType::LowPass:
        {
            const bool hp = p.type == FilterType::HighPass;
            if (order == 1)
            {
                add (matchedDesign (hp ? highpass1 (w0) : lowpass1 (w0), w0));
                break;
            }
            const int pairs = order / 2;
            for (int k = 1; k <= pairs; ++k)
            {
                double qk = (order == 2) ? q : butterworthQ (order, k);
                if (order > 2 && k == pairs)
                    qk *= q / kSqrtHalf; // resonance control on the highest-Q section
                add (matchedDesign (hp ? highpass2 (w0, qk) : lowpass2 (w0, qk), w0));
            }
            if (order % 2 == 1)
                add (matchedDesign (hp ? highpass1 (w0) : lowpass1 (w0), w0));
            break;
        }

        case FilterType::LowShelf:
        case FilterType::HighShelf:
        {
            const bool low = p.type == FilterType::LowShelf;
            if (order <= 2)
            {
                add (designShelf (low, order, w0, gainDb, q));
                break;
            }
            // Higher-order shelf: Butterworth-distributed Q values, gain split by section order.
            const int pairs = order / 2;
            for (int k = 1; k <= pairs; ++k)
            {
                double qk = butterworthQ (order, k);
                if (k == pairs)
                    qk *= q / kSqrtHalf;
                add (designShelf (low, 2, w0, gainDb * 2.0 / order, qk));
            }
            if (order % 2 == 1)
                add (designShelf (low, 1, w0, gainDb / order, q));
            break;
        }
    }

    return out;
}

} // namespace skjald::FilterDesign
