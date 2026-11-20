#include "../TestFramework.h"
#include "../TestSignals.h"
#include "Metering/LoudnessMeter.h"
#include "Metering/TruePeakMeter.h"
#include "Metering/KWeighting.h"

using namespace skjald;
using namespace skjald::test;

namespace
{
void feed (LoudnessMeter& m, const Buffer& b)
{
    const int n = (int) b[0].size();
    for (int s = 0; s < n; s += 1000)
    {
        const float* ptrs[2] { b[0].data() + s, b.size() > 1 ? b[1].data() + s : nullptr };
        m.process (ptrs, (int) b.size(), std::min (1000, n - s));
    }
}

Buffer concat (std::initializer_list<Buffer> parts)
{
    Buffer out = makeBuffer (2, 0);
    for (auto& p : parts)
        for (size_t c = 0; c < 2; ++c)
            out[c].insert (out[c].end(), p[c].begin(), p[c].end());
    return out;
}

Buffer tone (double seconds, double dbfs, double fs = 48000.0)
{
    return sine (2, (int) std::lround (seconds * fs), fs, 1000.0, std::pow (10.0, dbfs / 20.0));
}
} // namespace

TEST_CASE ("Loudness", "K-weighting coefficients match BS.1770 at 48 kHz")
{
    const auto k = KWeighting::design (48000.0);
    CHECK_NEAR (k.shelf.b0, 1.53512485958697, 1e-8);
    CHECK_NEAR (k.shelf.b1, -2.69169618940638, 1e-8);
    CHECK_NEAR (k.shelf.b2, 1.19839281085285, 1e-8);
    CHECK_NEAR (k.shelf.a1, -1.69065929318241, 1e-8);
    CHECK_NEAR (k.shelf.a2, 0.73248077421585, 1e-8);
    CHECK_NEAR (k.highpass.a1, -1.99004745483398, 1e-8);
    CHECK_NEAR (k.highpass.a2, 0.99007225036621, 1e-8);
}

TEST_CASE ("Loudness", "1 kHz at -23 dBFS reads -23 LUFS (momentary, short-term, integrated)")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        LoudnessMeter m;
        m.prepare (fs);
        feed (m, tone (20.0, -23.0, fs));
        CHECK_MSG (std::abs (m.getIntegrated() + 23.0f) < 0.1f, "fs %.0f integrated %.3f", fs, m.getIntegrated());
        CHECK_MSG (std::abs (m.getShortTerm() + 23.0f) < 0.1f, "fs %.0f short %.3f", fs, m.getShortTerm());
        CHECK_MSG (std::abs (m.getMomentary() + 23.0f) < 0.1f, "fs %.0f momentary %.3f", fs, m.getMomentary());
    }
}

TEST_CASE ("Loudness", "EBU Tech 3341 case 3: relative gate (-36/-23/-36 dBFS)")
{
    LoudnessMeter m;
    m.prepare (48000.0);
    feed (m, concat ({ tone (10.0, -36.0), tone (60.0, -23.0), tone (10.0, -36.0) }));
    CHECK_NEAR (m.getIntegrated(), -23.0, 0.1);
}

TEST_CASE ("Loudness", "EBU Tech 3341 case 4: absolute and relative gate (-72/-36/-23/-36/-72)")
{
    LoudnessMeter m;
    m.prepare (48000.0);
    feed (m, concat ({ tone (10.0, -72.0), tone (10.0, -36.0), tone (60.0, -23.0), tone (10.0, -36.0), tone (10.0, -72.0) }));
    CHECK_NEAR (m.getIntegrated(), -23.0, 0.1);
}

TEST_CASE ("Loudness", "EBU Tech 3341 case 5: level changes (-26/-20/-26 dBFS)")
{
    LoudnessMeter m;
    m.prepare (48000.0);
    feed (m, concat ({ tone (20.0, -26.0), tone (20.1, -20.0), tone (20.0, -26.0) }));
    CHECK_NEAR (m.getIntegrated(), -23.0, 0.1);
}

TEST_CASE ("Loudness", "Silence is gated, reset works")
{
    LoudnessMeter m;
    m.prepare (48000.0);
    feed (m, concat ({ tone (5.0, -18.0), makeBuffer (2, 48000 * 30) }));
    // 30 s of silence is gated out. The three 400 ms blocks that straddle the tone/silence
    // edge (75/50/25 % tone) are legitimately above the relative gate and count:
    // 47 full blocks + 1.5 block-equivalents over 50 blocks -> -18 + 10 log10(48.5/50)
    const double edge = 10.0 * std::log10 (48.5 / 50.0);
    CHECK_NEAR (m.getIntegrated(), -18.0 + edge, 0.02);
    CHECK (m.getMomentary() <= -90.0f);
    m.resetIntegrated();
    feed (m, tone (5.0, -30.0)); // the first blocks overlap the preceding silence the same way
    CHECK_NEAR (m.getIntegrated(), -30.0 + edge, 0.02);
}

TEST_CASE ("Loudness", "Momentary follows 400 ms window, short-term 3 s window")
{
    LoudnessMeter m;
    m.prepare (48000.0);
    feed (m, tone (5.0, -20.0));
    feed (m, tone (0.5, -40.0)); // after 500 ms of quiet tone the momentary window holds only it
    CHECK_NEAR (m.getMomentary(), -40.0, 0.2);
    // short-term window: 2.5 s at -20, 0.5 s at -40 -> energy mean
    const double expected = 10.0 * std::log10 ((2.5 * std::pow (10.0, -2.0) + 0.5 * std::pow (10.0, -4.0)) / 3.0);
    CHECK_NEAR (m.getShortTerm(), expected, 0.2);
}

TEST_CASE ("TruePeak", "Meter reads inter-sample peaks (EBU 3341-style sines)")
{
    // fs/4 sine with 45 degree phase: samples at 0.707 of the true peak (-3.01 dB)
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (double rel : { 0.25, 0.1, 0.4 })
        {
            TruePeakMeter m;
            m.prepare (fs);
            auto b = sine (2, (int) fs, fs, rel * fs, 0.5, kPi / 4.0);
            for (int i = 0; i < (int) (0.01 * fs); ++i) // 10 ms fade-in: no start-up click
                for (auto& c : b)
                    c[(size_t) i] *= (float) (0.5 - 0.5 * std::cos (kPi * i / (0.01 * fs)));
            const float* ptrs[2] { b[0].data(), b[1].data() };
            m.process (ptrs, 2, (int) fs);
            const double tp = toDb (m.getMaxPeak());
            CHECK_MSG (std::abs (tp - toDb (0.5)) < 0.12, "fs %.0f f %.2f fs: TP %.3f dB (expected %.3f)", fs, rel, tp, toDb (0.5));
        }
    }
}

TEST_CASE ("TruePeak", "Meter agrees with independent reconstruction on noise and clipped material")
{
    const double fs = 48000.0;
    Rng r (9);
    Buffer noise = makeBuffer (1, 40000);
    for (auto& v : noise[0])
        v = std::clamp (r.uniform() * 0.9f, -0.5f, 0.5f); // hard-clipped noise
    TruePeakMeter m;
    m.prepare (fs);
    const float* ptrs[1] { noise[0].data() };
    m.process (ptrs, 1, 40000);
    const double meter = toDb (m.getMaxPeak());
    const double indep = toDb (independentTruePeak (noise, 0, 0.0f));
    info ("meter %.3f dBTP, independent %.3f dBTP, sample peak %.3f dBFS", meter, indep, toDb (samplePeak (noise)));
    CHECK_NEAR (meter, indep, 0.15);
    CHECK (meter > toDb (samplePeak (noise)) + 1.0);
}
