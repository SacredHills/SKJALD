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
    {
        c.detector.prepare (kDetectorFactor, 70.0, TruePeakDetector::kDefaultEdge, true);
        c.bandLimit.design (kBandPassEdge, kBandStopEdge, kBandAttenuationDb);
    }
    detLatency = ch[0].detector.getLatency();
    bandLatency = ch[0].bandLimit.getLatency();
    for (auto& c : ch)
    {
        c.nyquistGuard.prepare (kDetectorFactor, kNyquistHalfLength, 60.0, kNyquistCutoff);
        c.nyquistAlign.prepare (std::max (1, detLatency - kNyquistHalfLength));
        c.ituAlign.prepare (std::max (1, detLatency - ItuPeakDetector::kLatency));
        c.samplePeakAlign.prepare (detLatency);
        c.peaks.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
        c.limited.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    }

    // The gain at output sample n is <= the required gain of every detector reading within
    // +-kNeighbourhood samples of it: hold length = window + 2K, signal delay = detector
    // latency + window - 1 + K (see process()).
    const int signalDelay = detLatency + window - 1 + kNeighbourhood;
    latency = bandLatency + signalDelay;
    for (auto& c : ch)
        c.signal.prepare (signalDelay);
    ceilingAlign.prepare (bandLatency + detLatency);

    hold.prepare (window + 2 * kNeighbourhood);
    hold.setLength (window + 2 * kNeighbourhood);
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
        c.bandLimit.reset();
        c.nyquistGuard.reset();
        c.itu.reset();
        c.nyquistAlign.reset();
        c.ituAlign.reset();
        c.samplePeakAlign.reset();
        c.signal.reset();
    }
    hold.reset();
    for (auto& b : boxes)
        b.reset (1.0f);
    env = 1.0f;
    blockMinGain = 1.0f;
    ceilingPrimed = false;
}

void TruePeakGuard::process (float* const* io, int numChannels, int numSamples, const float* ceiling, bool truePeak) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    numSamples = std::min (numSamples, (int) ch[0].peaks.size());
    float minGain = 1.0f;

    if (! ceilingPrimed && numSamples > 0)
    {
        ceilingAlign.reset (ceiling[0]);
        ceilingPrimed = true;
    }

    // 1. band-limit, 2. run the halfband detector on the band-limited signal (in both modes, so
    // switching true peak on is seamless)
    for (int c = 0; c < numChannels; ++c)
    {
        auto& st = ch[(size_t) c];
        for (int i = 0; i < numSamples; ++i)
            st.limited[(size_t) i] = st.bandLimit.process (io[c][i]);
        st.detector.process (st.limited.data(), st.peaks.data(), numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float r = 1.0f;
        const float ceilingLin = ceilingAlign.process (ceiling[i], bandLatency + detLatency);
        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];
            const float x = st.limited[(size_t) i];
            // All readings aligned to the same sample (detector latency); the guard uses the largest.
            const float nyq = st.nyquistAlign.process (st.nyquistGuard.processPeak (x), detLatency - kNyquistHalfLength);
            const float itu = st.ituAlign.process (st.itu.processPeak (x), detLatency - ItuPeakDetector::kLatency);
            const float sp = std::abs (st.samplePeakAlign.process (x, detLatency));
            const float tp = std::max (std::max (st.peaks[(size_t) i], nyq), std::max (itu, sp));
            const float det = truePeak ? tp : sp;
            if (det > ceilingLin)
                r = std::min (r, ceilingLin / det);
        }

        const float held = hold.process (r);
        env = held < env ? held : held + (env - held) * releaseCoeff;
        float g = boxes[0].process (env);
        g = boxes[1].process (g);
        minGain = std::min (minGain, g);

        const int signalDelay = detLatency + window - 1 + kNeighbourhood;
        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];
            io[c][i] = st.signal.process (st.limited[(size_t) i], signalDelay) * g;
        }
    }
    blockMinGain = minGain;
}

} // namespace skjald
