#include "../TestFramework.h"
#include "../TestSignals.h"

using namespace skjald;
using namespace skjald::test;

namespace
{
/** Loud kick-like bursts on the left, quiet steady tone on the right. */
Buffer lopsided (double fs, int n)
{
    auto b = makeBuffer (2, n);
    for (int i = 0; i < n; ++i)
    {
        const double t = std::fmod (i / fs, 0.25);
        b[0][(size_t) i] = (float) (0.95 * std::sin (kTwoPi * 80.0 * t) * std::exp (-t * 12.0));
        b[1][(size_t) i] = (float) (0.1 * std::sin (kTwoPi * 440.0 * i / fs));
    }
    return b;
}

double rightGainDb (float link, StereoLinkMode mode = StereoLinkMode::Standard)
{
    const double fs = 48000.0;
    const int n = (int) fs * 2;
    auto in = lopsided (fs, n);
    auto p = defaultParams();
    p.max.mode = LimiterMode::Transparent;
    p.max.thresholdDb = -10.0f;
    p.max.ceilingDb = -1.0f;
    p.max.stereoLink = link;
    p.max.linkMode = mode;
    MasteringEngine e;
    e.prepare (fs, 2);
    e.setParameters (p);
    auto out = in;
    processEngine (e, out);
    const int L = e.getLatency();
    // right channel: compare to the driven (+10 dB) input
    double so = 0.0, si = 0.0;
    for (int i = n / 2; i < n; ++i)
    {
        so += (double) out[1][(size_t) i] * out[1][(size_t) i];
        const double x = in[1][(size_t) (i - L)] * std::pow (10.0, 0.5);
        si += x * x;
    }
    return 10.0 * std::log10 (so / si);
}
} // namespace

TEST_CASE ("Stereo", "Stereo link: unlinked leaves the quiet channel alone, linked follows")
{
    const double unlinked = rightGainDb (0.0f);
    const double half = rightGainDb (0.5f);
    const double linked = rightGainDb (1.0f);
    info ("right-channel gain change: link 0 %.2f dB, 50 %% %.2f dB, 100 %% %.2f dB", unlinked, half, linked);
    CHECK_NEAR (unlinked, 0.0, 0.1);
    CHECK (linked < -3.0);
    CHECK (half < unlinked && half > linked);
}

TEST_CASE ("Stereo", "Mono processing and mono-compatible stereo")
{
    const double fs = 48000.0;
    auto stereo = syntheticMix ((int) fs * 2, fs, 1.0);
    for (auto& v : stereo[1]) (void) v;
    stereo[1] = stereo[0]; // identical channels
    Buffer mono { stereo[0] };

    auto p = defaultParams();
    p.max.thresholdDb = -8.0f;
    p.max.ceilingDb = -1.0f;

    MasteringEngine es, em;
    es.prepare (fs, 2);
    em.prepare (fs, 1);
    es.setParameters (p);
    em.setParameters (p);
    processEngine (es, stereo);
    processEngine (em, mono);

    double maxDiff = 0.0, lrDiff = 0.0;
    for (size_t i = 0; i < mono[0].size(); ++i)
    {
        maxDiff = std::max (maxDiff, (double) std::abs (mono[0][i] - stereo[0][i]));
        lrDiff = std::max (lrDiff, (double) std::abs (stereo[0][i] - stereo[1][i]));
    }
    CHECK_LE (lrDiff, 1.0e-7);
    CHECK_LE (maxDiff, 1.0e-5);
    CHECK_LE (toDb (samplePeak (mono)), -1.0 + 1.0e-4);
}

TEST_CASE ("Stereo", "Split link mode keeps the sustained stage linked")
{
    const double standardUnlinked = rightGainDb (0.0f, StereoLinkMode::Standard);
    const double splitUnlinked = rightGainDb (0.0f, StereoLinkMode::Split);
    info ("unlinked peak stage: standard %.2f dB, split %.2f dB", standardUnlinked, splitUnlinked);
    // Transparent has no leveler, so for this mode the two must match
    CHECK_NEAR (standardUnlinked, splitUnlinked, 0.01);
}
