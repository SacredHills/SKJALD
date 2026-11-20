#include "../TestFramework.h"
#include "../TestSignals.h"
#include "DSP/Filters/FilterDesign.h"
#include "DSP/Equalizer/Equalizer.h"

using namespace skjald;
using namespace skjald::test;

namespace
{
EQBandParams band (FilterType t, float f, float g, float q, int slope = 1)
{
    EQBandParams p;
    p.enabled = true;
    p.type = t;
    p.frequency = f;
    p.gainDb = g;
    p.q = q;
    p.slopeIndex = slope;
    return p;
}

double responseDb (const EQBandParams& p, double f, double fs)
{
    return 20.0 * std::log10 (FilterDesign::designBand (p, fs).magnitude (kTwoPi * f / fs));
}

/** Measures the actual processed gain of a sine through the Equalizer (steady state). */
double measuredGainDb (const EQParams& eqp, double f, double fs)
{
    Equalizer eq;
    eq.prepare (fs, 256);
    eq.setParameters (eqp);
    const int n = (int) (fs * 0.5);
    auto b = sine (2, n, fs, f, 0.25);
    const auto in = b;
    for (int s = 0; s < n; s += 256)
    {
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        eq.process (ptrs, 2, std::min (256, n - s));
    }
    return toDb (rms (b, n / 2) / rms (in, n / 2));
}
} // namespace

TEST_CASE ("EQ", "Bell gain accuracy at centre frequency")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (float f : { 20.0f, 100.0f, 1000.0f, 5000.0f, 15000.0f, 20000.0f })
            for (float g : { -24.0f, -6.0f, 3.0f, 12.0f, 24.0f })
                for (float q : { 0.1f, 0.7f, 4.0f, 10.0f })
                {
                    const double r = responseDb (band (FilterType::Bell, f, g, q), f, fs);
                    CHECK_MSG (std::abs (r - g) < 0.05, "fs %.0f f %.0f g %.1f q %.1f: %.3f dB", fs, f, g, q, r);
                }
}

TEST_CASE ("EQ", "Measured audio gain matches the design response")
{
    const double fs = 48000.0;
    EQParams p;
    p.bands[0] = band (FilterType::Bell, 1000.0f, 6.0f, 1.0f);
    p.bands[1] = band (FilterType::HighShelf, 8000.0f, -4.0f, 0.707f);
    p.bands[2] = band (FilterType::HighPass, 40.0f, 0.0f, 0.707f, 3);
    for (double f : { 30.0, 100.0, 1000.0, 3000.0, 12000.0 })
    {
        const double expected = 20.0 * std::log10 (Equalizer::magnitudeAt (p, f, fs));
        const double measured = measuredGainDb (p, f, fs);
        CHECK_MSG (std::abs (expected - measured) < 0.05, "f %.0f: measured %.3f expected %.3f", f, measured, expected);
    }
}

TEST_CASE ("EQ", "Q / bandwidth accuracy (bell half-gain points)")
{
    // RBJ definition: for a bell with gain G, the response is G/2 dB at f0 * (sqrt(1+1/(4Q^2)) +/- 1/(2Q))
    // when measured on the analog prototype. Check the digital design follows it below fs/8.
    const double fs = 96000.0;
    for (float q : { 0.5f, 1.0f, 2.0f, 5.0f })
    {
        const float f0 = 1000.0f, g = 12.0f;
        const double A = std::pow (10.0, g / 40.0);
        // half-dB-gain points of the RBJ analog bell: solve numerically
        const auto p = band (FilterType::Bell, f0, g, q);
        auto analogDb = [&] (double f)
        {
            const double w = f / f0;
            const double nr = 1.0 - w * w, ni = w * A / q;
            const double dr = 1.0 - w * w, di = w / (A * q);
            return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
        };
        for (double f : { f0 * 0.5, f0 * 0.8, f0 * 1.25, f0 * 2.0 })
            CHECK_MSG (std::abs (responseDb (p, f, fs) - analogDb (f)) < 0.05, "q %.1f f %.0f digital %.3f analog %.3f", q, f, responseDb (p, f, fs), analogDb (f));
    }
}

TEST_CASE ("EQ", "High-pass / low-pass slopes (6..48 dB/oct)")
{
    const double fs = 96000.0;
    for (int s = 0; s < 6; ++s)
    {
        const int dbPerOct = kSlopeChoicesDbPerOct[(size_t) s];
        const auto hp = band (FilterType::HighPass, 1000.0f, 0.0f, 0.7071f, s);
        const auto lp = band (FilterType::LowPass, 1000.0f, 0.0f, 0.7071f, s);
        CHECK_MSG (std::abs (responseDb (hp, 1000.0, fs) + 3.01) < 0.05, "HP %d dB/oct at fc: %.3f", dbPerOct, responseDb (hp, 1000.0, fs));
        CHECK_MSG (std::abs (responseDb (lp, 1000.0, fs) + 3.01) < 0.05, "LP %d dB/oct at fc: %.3f", dbPerOct, responseDb (lp, 1000.0, fs));
        // asymptotic slope measured between 2 and 3 octaves away from fc
        const double hpSlope = responseDb (hp, 250.0, fs) - responseDb (hp, 125.0, fs);
        const double lpSlope = responseDb (lp, 4000.0, fs) - responseDb (lp, 8000.0, fs);
        CHECK_MSG (std::abs (hpSlope - dbPerOct) < 0.6, "HP slope %d: measured %.2f dB/oct", dbPerOct, hpSlope);
        CHECK_MSG (std::abs (lpSlope - dbPerOct) < 0.6, "LP slope %d: measured %.2f dB/oct", dbPerOct, lpSlope);
    }
}

TEST_CASE ("EQ", "Shelf gain and midpoint, all slopes")
{
    const double fs = 48000.0;
    for (int s = 0; s < 6; ++s)
        for (float g : { -12.0f, 6.0f, 18.0f })
        {
            const auto ls = band (FilterType::LowShelf, 200.0f, g, 0.7071f, s);
            const auto hs = band (FilterType::HighShelf, 4000.0f, g, 0.7071f, s);
            CHECK_MSG (std::abs (responseDb (ls, 10.0, fs) - g) < 0.1, "LS slope %d g %.0f DC %.3f", s, g, responseDb (ls, 10.0, fs));
            CHECK_MSG (std::abs (responseDb (ls, 200.0, fs) - g / 2) < 0.2, "LS slope %d g %.0f mid %.3f", s, g, responseDb (ls, 200.0, fs));
            CHECK_MSG (std::abs (responseDb (ls, 10000.0, fs)) < 0.1, "LS slope %d g %.0f HF %.3f", s, g, responseDb (ls, 10000.0, fs));
            // 6 dB/oct shelves are still rising at 23 kHz: compare with the analog prototype there
            const double gl = std::pow (10.0, g / 20.0), W = 23000.0 / 4000.0;
            const double hfExpected = s == 0 ? 10.0 * std::log10 (gl * gl * (W * W + 1.0 / gl) / (W * W + gl)) : (double) g;
            CHECK_MSG (std::abs (responseDb (hs, 23000.0, fs) - hfExpected) < 0.3, "HS slope %d g %.0f HF %.3f (expected %.3f)", s, g, responseDb (hs, 23000.0, fs), hfExpected);
            CHECK_MSG (std::abs (responseDb (hs, 4000.0, fs) - g / 2) < 0.2, "HS slope %d g %.0f mid %.3f", s, g, responseDb (hs, 4000.0, fs));
            CHECK_MSG (std::abs (responseDb (hs, 50.0, fs)) < 0.1, "HS slope %d g %.0f LF %.3f", s, g, responseDb (hs, 50.0, fs));
        }
}

TEST_CASE ("EQ", "Notch depth and selectivity")
{
    const double fs = 48000.0;
    const auto n = band (FilterType::Notch, 3000.0f, 0.0f, 8.0f);
    CHECK_LE (responseDb (n, 3000.0, fs), -60.0);
    CHECK_NEAR (responseDb (n, 1500.0, fs), 0.0, 0.1);
    CHECK_NEAR (responseDb (n, 6000.0, fs), 0.0, 0.15);
}

TEST_CASE ("EQ", "Filter stability over the full parameter space")
{
    int unstable = 0, total = 0;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int t = 0; t < kNumFilterTypes; ++t)
            for (float f : { 20.0f, 31.5f, 63.0f, 125.0f, 500.0f, 2000.0f, 8000.0f, 16000.0f, 20000.0f })
                for (float g : { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f })
                    for (float q : { 0.1f, 0.5f, 0.707f, 2.0f, 10.0f })
                        for (int s = 0; s < 6; ++s)
                        {
                            const auto c = FilterDesign::designBand (band ((FilterType) t, f, g, q, s), fs);
                            for (int i = 0; i < c.numSections; ++i)
                            {
                                ++total;
                                const auto& sec = c.sections[(size_t) i];
                                const bool finite = std::isfinite (sec.b0) && std::isfinite (sec.b1) && std::isfinite (sec.b2)
                                                    && std::isfinite (sec.a1) && std::isfinite (sec.a2);
                                if (! finite || ! sec.isStable())
                                    ++unstable;
                            }
                        }
    info ("%d sections checked", total);
    CHECK (unstable == 0);
}

TEST_CASE ("EQ", "No cramping near Nyquist (matched design vs bilinear)")
{
    // A bilinear-transform (RBJ) bell high in the spectrum is severely narrowed. The matched
    // design is exact at f0 and follows the analog prototype far more closely elsewhere.
    // Residual error is inherent to a 2nd-order section whose analog response extends past
    // Nyquist; the GUI draws the true digital response, so the display stays truthful.
    const double fs = 44100.0;
    struct Case { float f0, g, q; double maxErr; };
    for (const Case c : { Case { 16000.0f, 9.0f, 1.0f, 1.2 }, Case { 12000.0f, 6.0f, 2.0f, 0.4 },
                          Case { 18000.0f, 12.0f, 0.7f, 2.0 }, Case { 10000.0f, -12.0f, 0.5f, 1.8 },
                          Case { 5000.0f, 6.0f, 0.5f, 0.35 }, Case { 2000.0f, -9.0f, 1.0f, 0.1 } })
    {
        const auto p = band (FilterType::Bell, c.f0, c.g, c.q);
        const double A = std::pow (10.0, c.g / 40.0);
        const double w0 = kTwoPi * c.f0 / fs;
        const double al = std::sin (w0) / (2.0 * c.q), a0 = 1.0 + al / A;
        BiquadCoeffs rbj;
        rbj.b0 = (1.0 + al * A) / a0; rbj.b1 = -2.0 * std::cos (w0) / a0; rbj.b2 = (1.0 - al * A) / a0;
        rbj.a1 = rbj.b1; rbj.a2 = (1.0 - al / A) / a0;
        double errMatched = 0.0, errBilinear = 0.0;
        for (double f = 20.0; f < 0.499 * fs; f *= 1.02)
        {
            const double w = f / c.f0;
            const double nr = 1.0 - w * w, ni = w * A / c.q, di = w / (A * c.q);
            const double analog = 10.0 * std::log10 ((nr * nr + ni * ni) / (nr * nr + di * di));
            errMatched = std::max (errMatched, std::abs (responseDb (p, f, fs) - analog));
            errBilinear = std::max (errBilinear, std::abs (20.0 * std::log10 (rbj.magnitude (kTwoPi * f / fs)) - analog));
        }
        info ("bell %5.0f Hz %+5.1f dB Q %.1f: max deviation matched %.2f dB, bilinear %.2f dB", c.f0, c.g, c.q, errMatched, errBilinear);
        CHECK_LE (errMatched, c.maxErr);
        CHECK (errMatched < 0.5 * errBilinear + 0.05);
        CHECK_NEAR (responseDb (p, c.f0, fs), c.g, 0.02);
    }
}

TEST_CASE ("EQ", "Stereo channels processed independently with linked parameters")
{
    const double fs = 48000.0;
    EQParams p;
    p.bands[0] = band (FilterType::Bell, 1000.0f, 6.0f, 1.0f);
    Equalizer eq;
    eq.prepare (fs, 256);
    eq.setParameters (p);
    auto b = makeBuffer (2, 4096);
    for (int i = 0; i < 4096; ++i)
    {
        b[0][(size_t) i] = (float) std::sin (kTwoPi * 1000.0 * i / fs) * 0.25f;
        b[1][(size_t) i] = 0.0f; // silent right channel must stay silent
    }
    for (int s = 0; s < 4096; s += 256)
    {
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        eq.process (ptrs, 2, 256);
    }
    float rightPeak = 0.0f;
    for (float v : b[1])
        rightPeak = std::max (rightPeak, std::abs (v));
    CHECK (rightPeak == 0.0f);
    CHECK_NEAR (toDb (samplePeak ({ std::vector<float> (b[0].begin() + 2048, b[0].end()) }) / 0.25), 6.0, 0.05);
}

TEST_CASE ("EQ", "Parameter smoothing: no clicks when gain, frequency and type change")
{
    const double fs = 48000.0;
    Equalizer eq;
    eq.prepare (fs, 256);
    EQParams p;
    p.bands[0] = band (FilterType::Bell, 1000.0f, 0.0f, 1.0f);
    eq.setParameters (p);

    const int n = (int) fs * 2;
    auto b = sine (2, n, fs, 440.0, 0.3);
    double maxStep = 0.0;
    float prev = 0.0f;
    for (int s = 0, block = 0; s < n; s += 256, ++block)
    {
        // jump parameters every 50 ms, including a structural type change
        if (block % 10 == 5)
        {
            p.bands[0].gainDb = (block / 10) % 2 ? 18.0f : -18.0f;
            p.bands[0].frequency = (block / 10) % 2 ? 600.0f : 2500.0f;
            p.bands[0].type = (block / 20) % 2 ? FilterType::LowShelf : FilterType::Bell;
        }
        eq.setParameters (p);
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        eq.process (ptrs, 2, std::min (256, n - s));
        for (int i = s; i < std::min (n, s + 256); ++i)
        {
            if (i > 0)
                maxStep = std::max (maxStep, (double) std::abs (b[0][(size_t) i] - prev));
            prev = b[0][(size_t) i];
        }
    }
    // A 440 Hz sine at up to +18 dB (2.4 peak) moves at most ~0.14 per sample;
    // a click would show up as a much larger step.
    info ("largest sample-to-sample step: %.4f", maxStep);
    CHECK_LE (maxStep, 0.25);
}

TEST_CASE ("EQ", "Bypass is bit-transparent")
{
    const double fs = 48000.0;
    EQParams p;
    p.bands[0] = band (FilterType::Bell, 1000.0f, 12.0f, 1.0f);
    p.bypass = true;
    Equalizer eq;
    eq.prepare (fs, 256);
    eq.setParameters (p);
    auto b = syntheticMix (4096, fs, 0.5);
    const auto in = b;
    for (int s = 0; s < 4096; s += 256)
    {
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        eq.process (ptrs, 2, 256);
    }
    CHECK (b == in);
}
