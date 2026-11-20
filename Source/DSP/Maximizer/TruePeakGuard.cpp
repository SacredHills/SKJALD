#include "TruePeakGuard.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void TruePeakGuard::prepare (double sampleRate, int maxBlockSize)
{
    window = std::max (8, (int) std::lround (0.001 * sampleRate)); // ~1 ms
    releaseCoeff = (float) onePoleCoeff (0.015, sampleRate);
    for (auto& c : ch)
        c.detector.prepare (kDetectorFactor, 70.0);
    const int detLatency = ch[0].detector.getLatency();
    for (auto& c : ch)
    {
        c.nyquistGuard.prepare (kDetectorFactor, kNyquistHalfLength, 60.0, kNyquistCutoff);
        c.nyquistAlign.prepare (std::max (1, detLatency - kNyquistHalfLength));
        c.samplePeakAlign.prepare (detLatency);
        c.peaks.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    }
    latency = detLatency + window - 1;
    for (auto& c : ch)
        c.signal.prepare (latency);

    hold.prepare (window);
    hold.setLength (window);
    const int total = window + 1; // two boxes: len1 + len2 - 1 == window
    boxes[0].prepare (window);
    boxes[1].prepare (window);
    boxes[0].setLength (total / 2 + (total % 2));
    boxes[1].setLength (total / 2);
    reset();
}

void TruePeakGuard::reset() noexcept
{
    for (auto& c : ch)
    {
        c.detector.reset();
        c.nyquistGuard.reset();
        c.nyquistAlign.reset();
        c.samplePeakAlign.reset();
        c.signal.reset();
    }
    hold.reset();
    for (auto& b : boxes)
        b.reset (1.0f);
    env = 1.0f;
    blockMinGain = 1.0f;
}

void TruePeakGuard::process (float* const* io, int numChannels, int numSamples, const float* ceiling, bool truePeak) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    numSamples = std::min (numSamples, (int) ch[0].peaks.size());
    const int detLatency = ch[0].detector.getLatency();
    float minGain = 1.0f;

    // Detector runs in both modes so switching true-peak on is seamless
    for (int c = 0; c < numChannels; ++c)
        ch[(size_t) c].detector.process (io[c], ch[(size_t) c].peaks.data(), numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        float r = 1.0f;
        const float ceilingLin = ceiling[i];
        for (int c = 0; c < numChannels; ++c)
        {
            const float x = io[c][i];
            auto& st = ch[(size_t) c];
            // Both detectors aligned to the same sample: the accurate halfband cascade and the
            // slightly over-reading near-Nyquist interpolator. The guard uses the larger one.
            const float nyq = st.nyquistAlign.process (st.nyquistGuard.processPeak (x), detLatency - kNyquistHalfLength);
            const float tp = std::max (st.peaks[(size_t) i], nyq);
            const float sp = std::abs (ch[(size_t) c].samplePeakAlign.process (x, detLatency));
            const float det = truePeak ? std::max (tp, sp) : sp;
            if (det > ceilingLin)
                r = std::min (r, ceilingLin / det);
        }

        const float held = hold.process (r);
        env = held < env ? held : held + (env - held) * releaseCoeff;
        float g = boxes[0].process (env);
        g = boxes[1].process (g);
        minGain = std::min (minGain, g);

        for (int c = 0; c < numChannels; ++c)
        {
            const float delayed = ch[(size_t) c].signal.process (io[c][i], latency);
            io[c][i] = delayed * g;
        }
    }
    blockMinGain = minGain;
}

} // namespace skjald
