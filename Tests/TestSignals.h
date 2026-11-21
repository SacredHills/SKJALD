#pragma once

// Reference signals and independent measurement helpers for the DSP tests.
// The measurement code here deliberately does NOT reuse the production DSP classes.

#include "DSP/Utilities/MathUtils.h"
#include "Engine/MasteringEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace skjald::test
{
using Buffer = std::vector<std::vector<float>>; // [channel][sample]

inline Buffer makeBuffer (int channels, int samples) { return Buffer ((size_t) channels, std::vector<float> ((size_t) samples, 0.0f)); }

struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    explicit Rng (uint64_t seed = 1) : s (seed * 0x9E3779B97F4A7C15ull + 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t) (s >> 11); }
    float uniform() { return (float) ((next() & 0xFFFFFF) / 16777216.0) * 2.0f - 1.0f; }
};

inline Buffer sine (int channels, int samples, double fs, double freq, double amplitude, double phase = 0.0)
{
    auto b = makeBuffer (channels, samples);
    for (int i = 0; i < samples; ++i)
    {
        const float v = (float) (amplitude * std::sin (kTwoPi * freq * i / fs + phase));
        for (auto& ch : b)
            ch[(size_t) i] = v;
    }
    return b;
}

/** Synthetic "mix": kick + snare-like noise bursts + bass + pads + hats, stereo,
    peak normalised to `peak`. Deterministic. */
inline Buffer syntheticMix (int samples, double fs, double peak = 1.0, uint64_t seed = 7)
{
    auto b = makeBuffer (2, samples);
    Rng rng (seed);
    const double beat = 0.5; // 120 BPM
    double snareEnv = 0.0, hatEnv = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const double t = i / fs;
        const double tb = std::fmod (t, beat);
        const int beatIndex = (int) (t / beat);

        // kick: pitch-swept sine with fast decay
        const double kf = 50.0 + 120.0 * std::exp (-tb * 30.0);
        const double kick = std::sin (kTwoPi * kf * tb) * std::exp (-tb * 9.0);

        // snare on 2 and 4: noise burst
        if (beatIndex % 2 == 1 && tb < 1.0 / fs * 1.5) snareEnv = 1.0;
        snareEnv *= std::exp (-1.0 / (0.06 * fs));
        const double snare = rng.uniform() * snareEnv * 0.7 + std::sin (kTwoPi * 190.0 * t) * snareEnv * 0.3;

        // hats on eighths
        const double te = std::fmod (t, beat / 2.0);
        if (te < 1.0 / fs * 1.5) hatEnv = 1.0;
        hatEnv *= std::exp (-1.0 / (0.015 * fs));
        const double hat = rng.uniform() * hatEnv * 0.25;

        const double bass = 0.35 * std::sin (kTwoPi * 55.0 * t) + 0.12 * std::sin (kTwoPi * 110.0 * t + 0.3);
        const double pad = 0.08 * (std::sin (kTwoPi * 220.0 * t) + std::sin (kTwoPi * 277.18 * t + 1.0) + std::sin (kTwoPi * 329.63 * t + 2.0));

        const double l = 0.9 * kick + 0.6 * snare + hat * 0.8 + bass + pad * 1.1;
        const double r = 0.9 * kick + 0.6 * snare + hat * 1.2 + bass + pad * 0.9;
        b[0][(size_t) i] = (float) l;
        b[1][(size_t) i] = (float) r;
    }
    float m = 0.0f;
    for (auto& ch : b)
        for (float v : ch)
            m = std::max (m, std::abs (v));
    const float g = (float) (peak / m);
    for (auto& ch : b)
        for (float& v : ch)
            v *= g;
    return b;
}

inline float samplePeak (const Buffer& b, int from = 0)
{
    float m = 0.0f;
    for (auto& ch : b)
        for (size_t i = (size_t) from; i < ch.size(); ++i)
            m = std::max (m, std::abs (ch[i]));
    return m;
}

/** Independent true-peak estimate: direct evaluation of a long (2 x 128 taps) Blackman-
    windowed sinc at 16 positions per sample interval (precomputed table). Intervals whose
    neighbouring samples are below `evalAbove` are skipped: pass a level at least ~10 dB
    below the levels under test (full-band noise can exceed its sample peaks by ~6 dB). */
inline double independentTruePeak (const Buffer& b, int from = 0, float evalAbove = 0.0f)
{
    constexpr int half = 128, ups = 16, taps = 2 * half;
    static const std::vector<double> table = []
    {
        std::vector<double> t ((size_t) (ups * taps));
        for (int p = 0; p < ups; ++p)
            for (int j = 0; j < taps; ++j)
            {
                const int k = j - half + 1;
                const double x = k - (double) p / ups;
                const double w = 0.42 + 0.5 * std::cos (kPi * x / half) + 0.08 * std::cos (2.0 * kPi * x / half);
                t[(size_t) (p * taps + j)] = sinc (x) * w;
            }
        return t;
    }();

    // Every local maximum of the 16x sequence is refined by a parabola through its neighbours
    // (16 points per sample alone can miss a crest near Nyquist by ~0.03 dB).
    auto refine = [] (double a, double bb, double c)
    {
        const double den = a - 2.0 * bb + c;
        if (bb >= a && bb >= c && den < 0.0)
            return bb - 0.125 * (a - c) * (a - c) / den;
        return bb;
    };
    double peak = 0.0;
    for (auto& ch : b)
    {
        const int n = (int) ch.size();
        for (int i = std::max (from, half); i < n - half - 1; ++i)
        {
            peak = std::max (peak, (double) std::abs (ch[(size_t) i]));
            if (std::max (std::abs (ch[(size_t) i]), std::abs (ch[(size_t) i + 1])) < evalAbove)
                continue;
            const float* base = ch.data() + (i - half + 1);
            double y[ups + 3]; // |y| at positions -1/16, 0, 1/16 .. 1, 1 + 1/16
            auto eval = [&] (const float* bp, int p)
            {
                const double* t = table.data() + p * taps;
                double acc = 0.0;
                for (int j = 0; j < taps; ++j)
                    acc += bp[j] * t[j];
                return std::abs (acc);
            };
            y[0] = eval (base - 1, ups - 1);
            y[1] = std::abs (ch[(size_t) i]);
            for (int p = 1; p < ups; ++p)
                y[p + 1] = eval (base, p);
            y[ups + 1] = std::abs (ch[(size_t) i + 1]);
            y[ups + 2] = eval (base + 1, 1);
            for (int k = 1; k <= ups + 1; ++k)
                peak = std::max (peak, refine (y[k - 1], y[k], y[k + 1]));
        }
    }
    return peak;
}

/** ITU-R BS.1770-4 Annex 2 reference true-peak meter (4x, the Recommendation's 48-tap
    filter), written independently of the production detector: this is what delivery
    platforms and most loudness meters measure. */
inline double ituTruePeak (const Buffer& b, int from = 0)
{
    static const double P[4][12] = {
        { 0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
          0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500 },
        { -0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
          0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375 },
        { -0.0189208984375, 0.0330810546875, -0.0582275390625, 0.1015625000000, -0.2003173828125, 0.7797851562500,
          0.4650878906250, -0.1665039062500, 0.0891113281250, -0.0517578125000, 0.0292968750000, -0.0291748046875 },
        { -0.0083007812500, 0.0148925781250, -0.0266113281250, 0.0476074218750, -0.1022949218750, 0.9721679687500,
          0.1373291015625, -0.0594482421875, 0.0332031250000, -0.0196533203125, 0.0109863281250, 0.0017089843750 } };
    double peak = 0.0;
    for (auto& ch : b)
        for (size_t n = (size_t) from + 11; n < ch.size(); ++n)
            for (const auto& ph : P)
            {
                double acc = 0.0;
                for (int j = 0; j < 12; ++j)
                    acc += ph[j] * ch[n - (size_t) j];
                peak = std::max (peak, std::abs (acc));
            }
    return peak;
}

/** Full-band white noise, hard clipped (a deliberately pathological true-peak signal:
    maximum energy right up to Nyquist and dense inter-sample overs). */
inline Buffer clippedNoise (int channels, int samples, double peak, uint64_t seed = 3)
{
    Rng rng { seed };
    auto b = makeBuffer (channels, samples);
    for (auto& ch : b)
        for (auto& v : ch)
            v = (float) (peak * std::clamp (2.0 * rng.uniform(), -1.0, 1.0));
    return b;
}

inline double rms (const Buffer& b, int from = 0, int to = -1)
{
    double s = 0.0;
    size_t count = 0;
    for (auto& ch : b)
    {
        const size_t end = to < 0 ? ch.size() : (size_t) to;
        for (size_t i = (size_t) from; i < end; ++i)
        {
            s += (double) ch[i] * ch[i];
            ++count;
        }
    }
    return count ? std::sqrt (s / count) : 0.0;
}

inline void processEngine (MasteringEngine& e, Buffer& b, int blockSize = 512)
{
    const int n = (int) b[0].size();
    const int chs = (int) b.size();
    for (int start = 0; start < n; start += blockSize)
    {
        const int len = std::min (blockSize, n - start);
        float* ptrs[2] { b[0].data() + start, chs > 1 ? b[1].data() + start : nullptr };
        e.process (ptrs, chs, len);
    }
}

inline EngineParams defaultParams()
{
    EngineParams p;
    for (int i = 0; i < kNumEQBands; ++i)
        p.eq.bands[(size_t) i].enabled = false;
    return p;
}

inline double toDb (double g) { return 20.0 * std::log10 (std::max (1.0e-12, g)); }

} // namespace skjald::test
