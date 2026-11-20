#include "EQBand.h"
#include "../Filters/FilterDesign.h"

#include <cmath>
#include <algorithm>

namespace skjald
{
namespace
{
    constexpr double kSmoothingSeconds = 0.05;
    constexpr double kCrossfadeSeconds = 0.03;
}

void EQBand::prepare (double sampleRate)
{
    fs = sampleRate;
    logFreq.reset (fs, kSmoothingSeconds);
    gainDb.reset (fs, kSmoothingSeconds);
    logQ.reset (fs, kSmoothingSeconds);
    fadeStep = 1.0 / std::max (1.0, kCrossfadeSeconds * fs);
    hasTarget = false;
    reset();
}

void EQBand::reset() noexcept
{
    active.resetState();
    old.resetState();
    fading = false;
    fadePos = 0.0;
    if (hasTarget)
    {
        logFreq.setCurrentAndTarget (std::log2 (target.frequency));
        gainDb.setCurrentAndTarget (target.gainDb);
        logQ.setCurrentAndTarget (std::log (target.q));
        active.cascade = FilterDesign::designBand (target, fs);
    }
}

EQBandParams EQBand::smoothedParams() const noexcept
{
    EQBandParams p = target;
    p.frequency = std::exp2 (logFreq.getCurrent());
    p.gainDb = gainDb.getCurrent();
    p.q = std::exp (logQ.getCurrent());
    return p;
}

void EQBand::redesignFromSmoothed() noexcept
{
    active.cascade = FilterDesign::designBand (smoothedParams(), fs);
}

void EQBand::setParameters (const EQBandParams& p) noexcept
{
    if (! hasTarget)
    {
        target = p;
        hasTarget = true;
        logFreq.setCurrentAndTarget (std::log2 (p.frequency));
        gainDb.setCurrentAndTarget (p.gainDb);
        logQ.setCurrentAndTarget (std::log (p.q));
        active.resetState();
        active.cascade = FilterDesign::designBand (p, fs);
        return;
    }

    const bool structural = ! target.sameStructure (p);
    target = p;
    logFreq.setTarget (std::log2 (p.frequency));
    gainDb.setTarget (p.gainDb);
    logQ.setTarget (std::log (p.q));

    if (structural)
    {
        // Crossfade from the current cascade (keeping its state) to the new structure.
        old = active;
        active.resetState();
        redesignFromSmoothed();
        fading = true;
        fadePos = 0.0;
    }
}

void EQBand::process (double* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);

    for (int start = 0; start < numSamples; start += kCoeffUpdateInterval)
    {
        const int len = std::min (kCoeffUpdateInterval, numSamples - start);

        if (logFreq.isSmoothing() || gainDb.isSmoothing() || logQ.isSmoothing())
        {
            logFreq.skip (len);
            gainDb.skip (len);
            logQ.skip (len);
            redesignFromSmoothed();
        }

        if (! fading)
        {
            if (active.cascade.numSections == 0)
                continue;
            for (int c = 0; c < numChannels; ++c)
            {
                double* d = channels[c] + start;
                for (int i = 0; i < len; ++i)
                    d[i] = active.run (c, d[i]);
            }
            continue;
        }

        double endPos = fadePos;
        for (int c = 0; c < numChannels; ++c)
        {
            double* d = channels[c] + start;
            double pos = fadePos;
            for (int i = 0; i < len; ++i)
            {
                const double x = d[i];
                const double yNew = active.run (c, x);
                const double yOld = old.run (c, x);
                pos = std::min (1.0, pos + fadeStep);
                d[i] = yOld + (yNew - yOld) * pos;
            }
            endPos = pos;
        }
        fadePos = endPos;
        if (fadePos >= 1.0)
        {
            fading = false;
            old.resetState();
        }
    }
}

} // namespace skjald
