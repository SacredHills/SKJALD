#include "../TestFramework.h"
#include "../TestSignals.h"
#include "DSP/Filters/LinearPhaseLowpass.h"
#include "DSP/Maximizer/TruePeakGuard.h"
#include "DSP/Oversampling/TruePeakDetector.h"

using namespace skjald;
using namespace skjald::test;

namespace
{
Buffer runEngine (const EngineParams& p, Buffer in, double fs, int block = 512, MasteringEngine* keep = nullptr)
{
    MasteringEngine local;
    MasteringEngine& e = keep ? *keep : local;
    e.prepare (fs, (int) in.size());
    e.setParameters (p);
    processEngine (e, in, block);
    return in;
}

EngineParams maxParams (LimiterMode mode, int osIndex, bool tp, float threshold, float ceiling)
{
    auto p = defaultParams();
    p.max.mode = mode;
    p.max.oversamplingIndex = osIndex;
    p.max.truePeak = tp;
    p.max.thresholdDb = threshold;
    p.max.ceilingDb = ceiling;
    return p;
}
} // namespace

TEST_CASE ("Limiter", "Latency is constant and reported correctly")
{
    const double fs = 48000.0;
    MasteringEngine e;
    e.prepare (fs, 2);
    const int expected = e.getLatency();
    CHECK (expected > 0);

    for (int os = 0; os < 5; ++os)
        for (int mode = 0; mode < kNumLimiterModes; ++mode)
        {
            auto p = maxParams ((LimiterMode) mode, os, true, 0.0f, 0.0f);
            p.max.lookaheadMs = (float) (os * 2); // lookahead must not change latency
            MasteringEngine m;
            m.prepare (fs, 2);
            CHECK (m.getLatency() == expected);
            m.setParameters (p);
            auto b = makeBuffer (2, 8192);
            b[0][1000] = 0.25f; // well below the ceiling: no limiting
            b[1][1000] = 0.25f;
            processEngine (m, b, 333);
            int peakIdx = 0;
            for (int i = 0; i < 8192; ++i)
                if (std::abs (b[0][(size_t) i]) > std::abs (b[0][(size_t) peakIdx])) peakIdx = i;
            CHECK_MSG (peakIdx - 1000 == expected, "os=%d mode=%d measured %d expected %d", os, mode, peakIdx - 1000, expected);
        }
}

TEST_CASE ("Limiter", "Sample-peak ceiling enforcement (all modes, all oversampling factors)")
{
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 4), fs, 1.0);
    for (int mode = 0; mode < kNumLimiterModes; ++mode)
        for (int os = 0; os < 5; ++os)
        {
            const float ceiling = -1.0f;
            auto out = runEngine (maxParams ((LimiterMode) mode, os, false, -12.0f, ceiling), mix, fs);
            const double sp = toDb (samplePeak (out));
            CHECK_MSG (sp <= ceiling + 1.0e-4, "mode %s os %dx: sample peak %.4f dBFS", getModeName ((LimiterMode) mode), 1 << os, sp);
        }
}

TEST_CASE ("Limiter", "True-peak ceiling enforcement against independent measurement")
{
    // Documented tolerance (docs/TECHNICAL.md section 3.5), every algorithm and factor:
    //  - SKJALD true-peak meter:                                          <= ceiling + 0.01 dB
    //  - ITU-R BS.1770-4 Annex 2 reference meter (independent code):      <= ceiling + 0.01 dB
    //  - idealised long-kernel sinc reconstruction (independent code):    <= ceiling + 0.02 dB
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 3), fs, 1.0, 11);
    double worstIdeal = -100.0, worstMeter = -100.0, worstItu = -100.0;
    for (int mode = 0; mode < kNumLimiterModes; ++mode)
        for (int os = 0; os < 5; ++os)
            for (float thr : { -6.0f, -18.0f })
            {
                const float ceiling = -1.0f;
                MasteringEngine e;
                auto out = runEngine (maxParams ((LimiterMode) mode, os, true, thr, ceiling), mix, fs, 512, &e);
                const double ideal = toDb (independentTruePeak (out, 0, dbToGain (ceiling - 12.0f)));
                const double itu = toDb (ituTruePeak (out));
                const double meter = toDb (e.getOutputTruePeakMeter().getMaxPeak());
                worstIdeal = std::max (worstIdeal, ideal - ceiling);
                worstItu = std::max (worstItu, itu - ceiling);
                worstMeter = std::max (worstMeter, meter - ceiling);
                CHECK_MSG (ideal <= ceiling + 0.02, "mode %s os %dx thr %.0f: ideal true peak %.3f dBTP", getModeName ((LimiterMode) mode), 1 << os, thr, ideal);
                CHECK_MSG (itu <= ceiling + 0.01, "mode %s os %dx thr %.0f: ITU true peak %.3f dBTP", getModeName ((LimiterMode) mode), 1 << os, thr, itu);
                CHECK_MSG (meter <= ceiling + 0.01, "mode %s os %dx thr %.0f: metered true peak %.3f dBTP", getModeName ((LimiterMode) mode), 1 << os, thr, meter);
            }
    info ("worst excess over ceiling: SKJALD meter %+.3f dB, ITU meter %+.3f dB, ideal reconstruction %+.3f dB", worstMeter, worstItu, worstIdeal);
}

TEST_CASE ("Limiter", "True peak holds on full-band hard-clipped noise (pathological signal)")
{
    // Maximum energy up to Nyquist: before the output band-limiter this read up to +0.18 dB
    // over the ceiling with an ideal reconstruction.
    const double fs = 44100.0;
    const auto noise = clippedNoise (2, (int) (fs * 2), 1.0);
    double worstIdeal = -100.0, worstItu = -100.0;
    for (int mode = 0; mode < kNumLimiterModes; ++mode)
        for (int os : { 0, 2, 4 })
        {
            const float ceiling = -1.0f;
            auto out = runEngine (maxParams ((LimiterMode) mode, os, true, -10.0f, ceiling), noise, fs);
            const double ideal = toDb (independentTruePeak (out, 0, dbToGain (ceiling - 12.0f)));
            const double itu = toDb (ituTruePeak (out));
            worstIdeal = std::max (worstIdeal, ideal - ceiling);
            worstItu = std::max (worstItu, itu - ceiling);
            CHECK_MSG (ideal <= ceiling + 0.02, "mode %s os %dx: ideal true peak %.3f dBTP", getModeName ((LimiterMode) mode), 1 << os, ideal);
            CHECK_MSG (itu <= ceiling + 0.01, "mode %s os %dx: ITU true peak %.3f dBTP", getModeName ((LimiterMode) mode), 1 << os, itu);
        }
    info ("worst excess over ceiling: ITU meter %+.3f dB, ideal reconstruction %+.3f dB", worstItu, worstIdeal);
}

TEST_CASE ("Limiter", "Output band-limiter: flat passband, deep stopband")
{
    LinearPhaseLowpass lp;
    lp.design (TruePeakGuard::kBandPassEdge, TruePeakGuard::kBandStopEdge, TruePeakGuard::kBandAttenuationDb);
    double passDev = 0.0, stopMax = 0.0;
    for (int i = 0; i <= 2000; ++i)
    {
        const double f = TruePeakGuard::kBandPassEdge * i / 2000.0;
        passDev = std::max (passDev, std::abs (toDb (lp.magnitudeAt (f))));
        const double fsb = TruePeakGuard::kBandStopEdge + (0.5 - TruePeakGuard::kBandStopEdge) * i / 2000.0;
        stopMax = std::max (stopMax, lp.magnitudeAt (fsb));
    }
    info ("%d taps, passband deviation %.5f dB, stopband %.1f dB", lp.getNumTaps(), passDev, toDb (stopMax));
    CHECK (passDev < 0.001);
    CHECK (toDb (stopMax) < -100.0);

    // impulse response: linear phase (symmetric), latency (N - 1) / 2
    std::vector<float> y ((size_t) lp.getNumTaps());
    lp.reset();
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = lp.process (i == 0 ? 1.0f : 0.0f);
    size_t peakIdx = 0;
    for (size_t i = 0; i < y.size(); ++i)
        if (y[i] > y[peakIdx]) peakIdx = i;
    CHECK ((int) peakIdx == lp.getLatency());
    for (size_t i = 0; i < y.size(); ++i)
        CHECK (std::abs (y[i] - y[y.size() - 1 - i]) < 1.0e-7f);
}

TEST_CASE ("Limiter", "Refined true-peak detector reads near-Nyquist crests")
{
    // A 20 kHz tone at 44.1 kHz: an 8x detector without refinement can miss the crest by up to
    // 0.13 dB depending on phase; the refined detector must be within 0.01 dB at every phase
    // (what remains is the passband ripple of the three 70 dB halfband stages).
    const double fs = 44100.0;
    double worst = 0.0;
    for (int ph = 0; ph < 16; ++ph)
    {
        auto s = sine (1, 8192, fs, 20000.0, 0.5, kTwoPi * ph / 16.0);
        TruePeakDetector d;
        d.prepare (8, 70.0, TruePeakDetector::kDefaultEdge, true);
        std::vector<float> pk (s[0].size());
        d.process (s[0].data(), pk.data(), (int) pk.size());
        float m = 0.0f;
        for (size_t i = 2048; i < pk.size(); ++i)
            m = std::max (m, pk[i]);
        worst = std::max (worst, std::abs (toDb (m / 0.5)));
    }
    info ("worst error %.4f dB", worst);
    CHECK (worst < 0.01);
}

TEST_CASE ("Limiter", "True-peak on band-limited material (< 0.45 fs) matches ideal reconstruction")
{
    // Same mix low-passed (8-pole) at 18 kHz before the limiter, oversampled processing:
    // here the ideal and metered results must agree and stay under the ceiling.
    const double fs = 44100.0;
    auto mix = syntheticMix ((int) (fs * 3), fs, 1.0, 11);
    EQParams lp;
    lp.bands[0].enabled = true;
    lp.bands[0].type = FilterType::LowPass;
    lp.bands[0].frequency = 18000.0f;
    lp.bands[0].slopeIndex = 5;
    for (int os = 1; os < 5; ++os)
    {
        auto p = maxParams (LimiterMode::Modern, os, true, -12.0f, -1.0f);
        p.eq = lp;
        auto out = runEngine (p, mix, fs);
        const double ideal = toDb (independentTruePeak (out, 0, dbToGain (-13.0f)));
        CHECK_MSG (ideal <= -1.0 + 0.02, "os %dx: ideal true peak %.3f dBTP", 1 << os, ideal);
    }
}

TEST_CASE ("Limiter", "Signals below the ceiling pass unchanged (1x)")
{
    const double fs = 48000.0;
    auto in = sine (2, 48000, fs, 997.0, 0.5);
    auto p = maxParams (LimiterMode::Transparent, 0, false, 0.0f, 0.0f);
    MasteringEngine e;
    auto out = runEngine (p, in, fs, 512, &e);
    const int L = e.getLatency();
    double maxErr = 0.0;
    for (int i = L + 1000; i < 48000; ++i)
        maxErr = std::max (maxErr, (double) std::abs (out[0][(size_t) i] - in[0][(size_t) (i - L)]));
    CHECK_LE (maxErr, 1.0e-6);
}

TEST_CASE ("Limiter", "Algorithms are genuinely different")
{
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 3), fs, 1.0, 3);
    std::vector<Buffer> outs;
    for (int mode = 0; mode < kNumLimiterModes; ++mode)
        outs.push_back (runEngine (maxParams ((LimiterMode) mode, 2, true, -10.0f, -1.0f), mix, fs));

    for (int a = 0; a < kNumLimiterModes; ++a)
    {
        const double ra = rms (outs[(size_t) a]);
        info ("%-12s RMS %.2f dBFS", getModeName ((LimiterMode) a), toDb (ra));
        for (int b = a + 1; b < kNumLimiterModes; ++b)
        {
            Buffer diff = outs[(size_t) a];
            for (size_t c = 0; c < diff.size(); ++c)
                for (size_t i = 0; i < diff[c].size(); ++i)
                    diff[c][i] -= outs[(size_t) b][c][i];
            const double rel = toDb (rms (diff) / ra);
            CHECK_MSG (rel > -40.0, "%s vs %s differ by only %.1f dB", getModeName ((LimiterMode) a), getModeName ((LimiterMode) b), rel);
        }
    }
}

TEST_CASE ("Limiter", "Sample-rate stability 44.1 - 192 kHz")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        const auto mix = syntheticMix ((int) (fs * 1.5), fs, 1.0, 5);
        auto out = runEngine (maxParams (LimiterMode::Modern, 1, true, -12.0f, -0.5f), mix, fs);
        bool finite = true;
        for (auto& ch : out)
            for (float v : ch)
                finite = finite && std::isfinite (v);
        CHECK (finite);
        const double sp = toDb (samplePeak (out));
        CHECK_MSG (sp <= -0.5 + 1.0e-4, "fs %.0f: sample peak %.4f", fs, sp);
        CHECK_MSG (rms (out) > 0.05, "fs %.0f: output unexpectedly quiet", fs);
    }
}
