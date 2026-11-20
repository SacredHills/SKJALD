#pragma once

#include "Biquad.h"
#include "../Equalizer/EQTypes.h"
#include <cmath>

namespace skjald::FilterDesign
{
/** Analog prototype section H(s) = (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0),
    with s in radians/sample (so analog and digital frequency axes coincide). */
struct AnalogSection
{
    double n0 = 1.0, n1 = 0.0, n2 = 0.0;
    double d0 = 1.0, d1 = 0.0, d2 = 0.0;

    double magnitudeSquared (double omega) const noexcept;
    bool isSecondOrder() const noexcept { return std::abs (d2) > 0.0; }
};

/** Magnitude-matched digital design (impulse-invariant poles + numerator fitted so the
    digital magnitude equals the analog magnitude at DC, at `omegaMatch` and at Nyquist).
    This follows the approach published by M. Vicanek ("Matched Second Order Digital
    Filters", 2016) and avoids the bilinear transform's cramping near Nyquist.
    The numerator is always returned minimum-phase. */
BiquadCoeffs matchedDesign (const AnalogSection& a, double omegaMatch) noexcept;

/** Returns 1/H for a section whose numerator is minimum-phase. */
BiquadCoeffs invert (const BiquadCoeffs& c) noexcept;

/** Reflect any numerator zeros outside the unit circle inside (magnitude preserved). */
BiquadCoeffs makeNumeratorMinimumPhase (const BiquadCoeffs& c) noexcept;

/** Designs the complete section cascade for one EQ band. Thread-safe and allocation free:
    used both by the audio thread and by the GUI for response drawing, so the drawn curve is
    computed from exactly the same coefficients that process audio. */
SectionCascade designBand (const EQBandParams& p, double sampleRate) noexcept;

/** Maximum design frequency relative to the sample rate (keeps poles below Nyquist). */
constexpr double kMaxRelativeFrequency = 0.49;

} // namespace skjald::FilterDesign
