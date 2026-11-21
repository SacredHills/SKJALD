#include "../TestFramework.h"
#include "../TestSignals.h"
#include "../../Source/Engine/LearnThreshold.h"

#include <chrono>

using namespace skjald;
using namespace skjald::test;

namespace
{
EngineParams base (LimiterMode mode = LimiterMode::Transparent)
{
    auto p = defaultParams();
    p.max.mode = mode;
    p.max.thresholdDb = -10.0f;
    p.max.ceilingDb = -1.0f;
    p.max.adaptiveRelease = false;
    return p;
}

Buffer run (const EngineParams& p, Buffer b, double fs, MasteringEngine* keep = nullptr, int block = 512)
{
    MasteringEngine local;
    MasteringEngine& e = keep ? *keep : local;
    e.prepare (fs, (int) b.size());
    e.setParameters (p);
    processEngine (e, b, block);
    return b;
}

/** Gain trajectory after a single loud burst: returns ms needed to recover to -1 dB GR. */
double recoveryMs (const EngineParams& p, double fs)
{
    const int n = (int) (fs * 3);
    auto b = makeBuffer (2, n);
    // steady 0.25 sine with a 20 ms burst at 4x the level
    for (int i = 0; i < n; ++i)
    {
        const double a = (i > fs && i < fs + 0.02 * fs) ? 1.0 : 0.25;
        const float v = (float) (a * std::sin (kTwoPi * 200.0 * i / fs));
        b[0][(size_t) i] = b[1][(size_t) i] = v;
    }
    auto pp = p;
    pp.max.thresholdDb = 0.0f;
    pp.max.ceilingDb = -6.0f; // burst (0 dB) needs 6 dB GR, the sine (-12 dB) needs none
    MasteringEngine e;
    auto out = run (pp, b, fs, &e);
    const int L = e.getLatency();
    // envelope ratio measured per 2.5 ms window after the burst
    const int win = (int) (fs * 0.0025);
    for (int s = (int) (fs + 0.02 * fs) + L; s + win < n; s += win)
    {
        double eo = 0.0, ei = 0.0;
        for (int i = s; i < s + win; ++i)
        {
            eo += (double) out[0][(size_t) i] * out[0][(size_t) i];
            ei += (double) b[0][(size_t) (i - L)] * b[0][(size_t) (i - L)];
        }
        if (10.0 * std::log10 (eo / ei) > -1.0)
            return (s - (fs + 0.02 * fs) - L) / fs * 1000.0;
    }
    return 1.0e9;
}
} // namespace

TEST_CASE ("Maximizer", "Release time controls recovery")
{
    const double fs = 48000.0;
    auto fast = base();
    fast.max.releaseMs = 20.0f;
    auto slow = base();
    slow.max.releaseMs = 400.0f;
    const double rf = recoveryMs (fast, fs), rs = recoveryMs (slow, fs);
    info ("recovery to -1 dB: release 20 ms -> %.1f ms, release 400 ms -> %.1f ms", rf, rs);
    CHECK (rf < 120.0);
    CHECK (rs > 4.0 * rf);
}

TEST_CASE ("Maximizer", "Auto release (ARC): isolated peaks recover fast, ceiling holds")
{
    const double fs = 48000.0;
    auto autoP = base();
    autoP.max.autoRelease = true;
    const double ra = recoveryMs (autoP, fs);
    auto slow = base();
    slow.max.releaseMs = 400.0f;
    const double rs = recoveryMs (slow, fs);
    info ("isolated burst recovery: auto %.1f ms, manual 400 ms release %.1f ms", ra, rs);
    CHECK (ra < 150.0);
    CHECK (ra < 0.25 * rs);
}

TEST_CASE ("Maximizer", "Auto release (ARC): dense material is louder than a slow manual release and never over the ceiling")
{
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 10), fs, 1.0, 21);
    auto autoP = base (LimiterMode::Modern);
    autoP.max.thresholdDb = -10.0f;
    autoP.max.autoRelease = true;
    auto slow = autoP;
    slow.max.autoRelease = false;
    slow.max.releaseMs = 250.0f;
    MasteringEngine ea, es;
    run (autoP, mix, fs, &ea);
    run (slow, mix, fs, &es);
    const double la = ea.getOutputLoudness().getIntegrated(), ls = es.getOutputLoudness().getIntegrated();
    info ("integrated loudness: auto %.2f LUFS, manual 250 ms %.2f LUFS", la, ls);
    CHECK (la > ls);
    CHECK_LE (toDb (ea.getOutputTruePeakMeter().getMaxPeak()), -1.0 + 0.01);
}

TEST_CASE ("Maximizer", "Adaptive release: short peaks recover faster than the manual setting")
{
    const double fs = 48000.0;
    auto manual = base();
    manual.max.releaseMs = 300.0f;
    auto adaptive = manual;
    adaptive.max.adaptiveRelease = true;
    const double rm = recoveryMs (manual, fs), ra = recoveryMs (adaptive, fs);
    info ("isolated burst recovery: manual %.1f ms, adaptive %.1f ms", rm, ra);
    CHECK (ra < 0.8 * rm);
}

TEST_CASE ("Maximizer", "Transient emphasis and Punch preserve drum attacks")
{
    // Metric: transient-to-body ratio (TBR) of each drum hit in the limited output:
    // energy of the first 6 ms after the onset vs. the energy 30-200 ms after it.
    // A peak limiter lowers TBR (the peak is pinned, the body is not); transient
    // preservation means TBR stays higher at the same threshold.
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 4.2), fs, 1.0, 21);
    auto tbr = [&] (const EngineParams& p, bool limited = true)
    {
        Buffer out = mix;
        int L = 0;
        if (limited)
        {
            MasteringEngine e;
            out = run (p, mix, fs, &e);
            L = e.getLatency();
        }
        auto energy = [&] (int a, int b)
        {
            double s = 0.0;
            for (int i = a; i < b; ++i)
                for (size_t c = 0; c < 2; ++c)
                    s += (double) out[c][(size_t) (i + L)] * out[c][(size_t) (i + L)];
            return s / (b - a);
        };
        double total = 0.0;
        int hits = 0;
        for (double t = 1.0; t < 4.0; t += 0.5)
        {
            const int onset = (int) (t * fs);
            total += 10.0 * std::log10 (energy (onset, onset + (int) (0.006 * fs)) / energy (onset + (int) (0.03 * fs), onset + (int) (0.2 * fs)));
            ++hits;
        }
        return total / hits;
    };
    auto pT = base (LimiterMode::Transparent);
    pT.max.thresholdDb = -8.0f;
    pT.max.adaptiveRelease = true;
    auto pP = pT;
    pP.max.mode = LimiterMode::Punch;
    auto pE = pT;
    pE.max.transientEmphasis = 1.0f;
    const double tIn = tbr (pT, false), tT = tbr (pT), tP = tbr (pP), tE = tbr (pE);
    info ("transient-to-body ratio: input %.2f dB, Transparent %.2f dB, Punch %.2f dB, Transparent + 100%% emphasis %.2f dB", tIn, tT, tP, tE);
    // The thresholds were recalibrated when the limiter became frequency-aware: Transparent alone now
    // keeps 3.8 dB of the input's 6.6 dB (it kept 2.1 dB before), so the room left for Punch and
    // Transient Emphasis to add is smaller (they reach 5.4 and 5.0 dB).
    CHECK (tT < tIn);         // limiting reduces punch...
    // The always-on soft clipper keeps more of the attack in Transparent too (3.9 dB now), so the
    // margin that Punch (4.6 dB) and Transient Emphasis (4.7 dB) add on top is about 0.8 dB.
    CHECK (tP > tT + 0.5);    // ...Punch keeps clearly more of it
    CHECK (tE > tT + 0.5);    // ...and so does Transient Emphasis
}

TEST_CASE ("Maximizer", "Soft clipping is always on, keeps the ceiling and ignores the old switch")
{
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) (fs * 2), fs, 1.0, 4);
    auto p = base (LimiterMode::Modern);
    p.max.thresholdDb = -12.0f;
    p.max.truePeak = false;
    auto a = run (p, mix, fs);
    p.max.softClip = true; // legacy parameter, no longer has any effect
    auto b = run (p, mix, fs);
    Buffer d = a;
    for (size_t c = 0; c < 2; ++c)
        for (size_t i = 0; i < d[c].size(); ++i)
            d[c][i] -= b[c][i];
    CHECK_MSG (rms (d) == 0.0, "legacy Soft Clip switch must not change the result");
    CHECK_LE (toDb (samplePeak (b)), -1.0 + 1.0e-4);
}

TEST_CASE ("Maximizer", "Delta monitoring is latency aligned (wet + delta == driven input)")
{
    const double fs = 48000.0;
    const auto mix = syntheticMix ((int) (fs * 1.5), fs, 0.9, 8);
    auto p = base (LimiterMode::Modern);
    p.max.thresholdDb = -6.0f;
    p.max.oversamplingIndex = 0; // 1x: no reconstruction filter in the wet path
    MasteringEngine e1, e2;
    auto wet = run (p, mix, fs, &e1);
    p.max.delta = true;
    auto delta = run (p, mix, fs, &e2);
    const int L = e1.getLatency();
    const float drive = dbToGain (6.0f);
    double maxErr = 0.0;
    for (size_t i = (size_t) L + 100; i < mix[0].size(); ++i)
        for (size_t c = 0; c < 2; ++c)
            maxErr = std::max (maxErr, (double) std::abs (wet[c][i] + delta[c][i] - mix[c][i - (size_t) L] * drive));
    info ("max |wet + delta - driven input| = %.2e", maxErr);
    CHECK_LE (maxErr, 1.0e-5);
}

TEST_CASE ("Maximizer", "Latency-compensated bypass (maximizer and global)")
{
    const double fs = 48000.0;
    const auto mix = syntheticMix ((int) fs, fs, 0.9, 2);
    for (int which = 0; which < 2; ++which)
    {
        auto p = base (LimiterMode::Aggressive);
        if (which == 0) p.max.bypass = true;
        else p.globalBypass = true;
        p.eq.bands[0].enabled = which == 0; // EQ active but neutral when only the maximizer is bypassed
        p.eq.bands[0].gainDb = 0.0f;
        MasteringEngine e;
        auto out = run (p, mix, fs, &e);
        const int L = e.getLatency();
        double maxErr = 0.0;
        for (size_t i = (size_t) L; i < mix[0].size(); ++i)
            for (size_t c = 0; c < 2; ++c)
                maxErr = std::max (maxErr, (double) std::abs (out[c][i] - mix[c][i - (size_t) L]));
        CHECK_MSG (maxErr < 1.0e-6, "%s bypass error %.2e", which == 0 ? "maximizer" : "global", maxErr);
    }
}

TEST_CASE ("Maximizer", "Automatic gain compensation lowers loudness and never exceeds the ceiling")
{
    const double fs = 48000.0;
    const auto mix = syntheticMix ((int) (fs * 8), fs, 0.5, 13);
    auto p = base (LimiterMode::Modern);
    p.max.thresholdDb = -12.0f;
    MasteringEngine e1, e2;
    auto plain = run (p, mix, fs, &e1);
    p.max.autoGain = true;
    auto agc = run (p, mix, fs, &e2);
    const double inLufs = e2.getInputLoudness().getShortTerm();
    const double plainLufs = e1.getOutputLoudness().getShortTerm();
    const double agcLufs = e2.getOutputLoudness().getShortTerm();
    info ("short-term loudness: input %.2f, limited %.2f, limited+AGC %.2f LUFS", inLufs, plainLufs, agcLufs);
    CHECK (std::abs (agcLufs - inLufs) < 1.0);
    CHECK (agcLufs < plainLufs - 3.0);
    CHECK_LE (toDb (e2.getOutputTruePeakMeter().getMaxPeak()), -1.0 + 0.01);
}

TEST_CASE ("Maximizer", "Learn Threshold lands the output near the target loudness")
{
    const double fs = 48000.0;
    for (double level : { 0.3, 0.12 })
    {
        const auto mix = syntheticMix ((int) (fs * 20), fs, level, 21);
        for (float target : { -16.0f, -14.0f, -12.0f })
        {
            MasteringEngine probe;
            auto p = base (LimiterMode::Modern);
            p.max.thresholdDb = 0.0f;
            run (p, mix, fs, &probe);
            const float inLufs = probe.getInputLoudness().getIntegrated();
            const float peak = toDb (probe.getInputPeakMeter().getMaxPeak());
            p.max.thresholdDb = learnedThresholdDb (inLufs, target, 0.0f, peak, p.max.ceilingDb);
            MasteringEngine e;
            run (p, mix, fs, &e);
            const float outLufs = e.getOutputLoudness().getIntegrated();
            info ("level %.2f target %.1f: input %.2f (peak %.1f), threshold %.2f, output %.2f LUFS", level, target, inLufs, peak,
                  p.max.thresholdDb, outLufs);
            CHECK (std::abs (outLufs - target) < 1.0);
        }
    }
}

TEST_CASE ("Maximizer", "Learn Threshold clamps to the parameter range")
{
    CHECK_MSG (learnedThresholdDb (-25.0f, -3.0f, 0.0f, -10.0f, -1.0f) == -30.0f, "unreachable target must clamp to -30 dB");
    CHECK_MSG (learnedThresholdDb (-10.0f, -30.0f, 0.0f, -3.0f, -1.0f) == 0.0f, "quieter target must clamp to 0 dB");
    CHECK_MSG (learnedThresholdDb (-80.0f, -14.0f, 0.0f, -90.0f, -1.0f) == 0.0f, "nothing measured must leave unity drive");
}

TEST_CASE ("Maximizer", "Oversampling switch mid-stream: no gap, no click, ceiling kept")
{
    const double fs = 48000.0;
    const int n = (int) (fs * 3);
    auto b = sine (2, n, fs, 150.0, 0.9);
    auto p = base (LimiterMode::Modern);
    p.max.thresholdDb = -3.0f;
    p.max.truePeak = false;
    MasteringEngine e;
    e.prepare (fs, 2);
    e.setParameters (p);
    double maxStep = 0.0, minRmsWin = 1.0e9;
    float prev = 0.0f;
    for (int s = 0, blk = 0; s < n; s += 480, ++blk)
    {
        if (blk % 25 == 0)
        {
            p.max.oversamplingIndex = (blk / 25) % 5; // walk through 1x..16x
            e.setParameters (p);
        }
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        e.process (ptrs, 2, std::min (480, n - s));
        if (s > (int) fs / 2)
        {
            double sq = 0.0;
            for (int i = s; i < std::min (n, s + 480); ++i)
            {
                maxStep = std::max (maxStep, (double) std::abs (b[0][(size_t) i] - prev));
                prev = b[0][(size_t) i];
                sq += (double) b[0][(size_t) i] * b[0][(size_t) i];
            }
            minRmsWin = std::min (minRmsWin, std::sqrt (sq / 480));
        }
        else
            prev = b[0][(size_t) std::min (n, s + 480) - 1];
    }
    info ("largest step %.4f, quietest 10 ms window RMS %.3f", maxStep, minRmsWin);
    CHECK_LE (maxStep, 0.03);        // 150 Hz at 0.9 peak moves <= 0.018/sample
    CHECK (minRmsWin > 0.5);         // no dropout
    CHECK_LE (toDb (samplePeak (b)), -1.0 + 1.0e-4);
}

TEST_CASE ("Maximizer", "Parameter changes are click-free (threshold, ceiling, mode)")
{
    const double fs = 48000.0;
    const int n = (int) (fs * 2);
    auto b = sine (2, n, fs, 100.0, 0.5);
    auto p = base (LimiterMode::Transparent);
    MasteringEngine e;
    e.prepare (fs, 2);
    e.setParameters (p);
    double maxStep = 0.0;
    float prev = 0.0f;
    for (int s = 0, blk = 0; s < n; s += 256, ++blk)
    {
        if (blk % 20 == 10)
        {
            p.max.thresholdDb = (blk / 20) % 2 ? -18.0f : -2.0f;
            p.max.ceilingDb = (blk / 20) % 2 ? -3.0f : -0.3f;
            p.max.mode = (LimiterMode) ((blk / 20) % kNumLimiterModes);
            e.setParameters (p);
        }
        float* ptrs[2] { b[0].data() + s, b[1].data() + s };
        e.process (ptrs, 2, std::min (256, n - s));
        for (int i = s; i < std::min (n, s + 256); ++i)
        {
            if (i > (int) fs / 4)
                maxStep = std::max (maxStep, (double) std::abs (b[0][(size_t) i] - prev));
            prev = b[0][(size_t) i];
        }
    }
    info ("largest sample-to-sample step: %.4f", maxStep);
    CHECK_LE (maxStep, 0.03); // 100 Hz at <= 0.9 peak: <= 0.012/sample
}

TEST_CASE ("Maximizer", "Variable and odd host block sizes give identical output")
{
    const double fs = 44100.0;
    const auto mix = syntheticMix ((int) fs, fs, 1.0, 17);
    auto p = base (LimiterMode::Punch);
    p.max.thresholdDb = -9.0f;
    auto ref = run (p, mix, fs, nullptr, 512);
    for (int block : { 1, 7, 64, 333, 1024, 4096 })
    {
        auto out = run (p, mix, fs, nullptr, block);
        double maxErr = 0.0;
        for (size_t c = 0; c < 2; ++c)
            for (size_t i = 0; i < out[c].size(); ++i)
                maxErr = std::max (maxErr, (double) std::abs (out[c][i] - ref[c][i]));
        CHECK_MSG (maxErr < 1.0e-6, "block %d differs by %.2e", block, maxErr);
    }
}

TEST_CASE ("Maximizer", "Sustained loud signal and silence: stable, no denormals, no NaN")
{
    const double fs = 48000.0;
    auto b = sine (2, (int) fs * 3, fs, 60.0, 1.0);
    for (size_t i = (size_t) fs * 2; i < b[0].size(); ++i)
        b[0][i] = b[1][i] = 1.0e-30f; // tiny values after a loud passage
    auto p = base (LimiterMode::Smooth);
    p.max.thresholdDb = -20.0f;
    auto out = run (p, b, fs);
    bool finite = true;
    for (auto& ch : out)
        for (float v : ch)
            finite = finite && std::isfinite (v);
    CHECK (finite);
    CHECK_LE (toDb (samplePeak (out)), -1.0 + 1.0e-4);
}

TEST_CASE ("Performance", "Realtime factor of the complete chain")
{
    const double fs = 48000.0;
    const auto mix = syntheticMix ((int) (fs * 10), fs, 1.0, 1);
    auto p = base (LimiterMode::Modern);
    for (int i = 0; i < 8; ++i)
    {
        p.eq.bands[(size_t) i].enabled = true;
        p.eq.bands[(size_t) i].type = FilterType::Bell;
        p.eq.bands[(size_t) i].frequency = 60.0f * std::pow (2.0f, (float) i);
        p.eq.bands[(size_t) i].gainDb = 2.0f;
    }
    p.max.truePeak = true;
    for (int os = 0; os < 5; ++os)
    {
        p.max.oversamplingIndex = os;
        MasteringEngine e;
        e.prepare (fs, 2);
        e.setParameters (p);
        auto b = mix;
        const auto t0 = std::chrono::steady_clock::now();
        processEngine (e, b, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double rt = 10.0 / secs;
        info ("%2dx oversampling, 8 EQ bands, true peak: %.1fx realtime (%.2f %% of one core)", 1 << os, rt, 100.0 / rt);
        CHECK (rt > 4.0); // generous bound for slow CI machines
    }
}
