#include "Equalizer.h"
#include "../Filters/FilterDesign.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>

namespace skjald
{
void Equalizer::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = std::max (1, maxBlockSize);
    for (auto& b : bands)
        b.prepare (sampleRate);
    for (auto& s : scratch)
        s.assign ((size_t) maxBlock, 0.0);
    for (auto& d : dry)
        d.assign ((size_t) maxBlock, 0.0f);
    wet.reset (sampleRate, 0.02);
    wet.setCurrentAndTarget (1.0f);
    firstParams = true;
}

void Equalizer::reset() noexcept
{
    for (auto& b : bands)
        b.reset();
}

void Equalizer::setParameters (const EQParams& p) noexcept
{
    for (size_t i = 0; i < bands.size(); ++i)
        bands[i].setParameters (p.bands[i]);
    if (firstParams)
        wet.setCurrentAndTarget (p.bypass ? 0.0f : 1.0f); // no fade on the very first update
    else
        wet.setTarget (p.bypass ? 0.0f : 1.0f);
    firstParams = false;
}

void Equalizer::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);
    numSamples = std::min (numSamples, maxBlock);

    const bool fullyBypassed = ! wet.isSmoothing() && wet.getTarget() == 0.0f;
    if (fullyBypassed)
    {
        // Filter states are frozen while bypassed; the 20 ms fade-in on re-enable masks
        // the restart, and skipping the work keeps a bypassed EQ free of CPU cost.
        return;
    }

    bool anyActive = false;
    for (auto& b : bands)
        anyActive = anyActive || b.isActive();
    if (! anyActive)
        return;

    double* ptrs[kMaxChannels] {};
    for (int c = 0; c < numChannels; ++c)
    {
        ptrs[c] = scratch[(size_t) c].data();
        std::copy (channels[c], channels[c] + numSamples, dry[(size_t) c].begin());
        for (int i = 0; i < numSamples; ++i)
            ptrs[c][i] = (double) channels[c][i];
    }

    for (auto& b : bands)
        if (b.isActive())
            b.process (ptrs, numChannels, numSamples);

    if (! wet.isSmoothing() && wet.getTarget() == 1.0f)
    {
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < numSamples; ++i)
                channels[c][i] = (float) ptrs[c][i];
        return;
    }

    // Bypass crossfade
    for (int i = 0; i < numSamples; ++i)
    {
        const float w = wet.next();
        for (int c = 0; c < numChannels; ++c)
        {
            const float d = dry[(size_t) c][(size_t) i];
            channels[c][i] = d + ((float) ptrs[c][i] - d) * w;
        }
    }
}

double Equalizer::magnitudeAt (const EQParams& p, double frequencyHz, double sampleRate) noexcept
{
    if (p.bypass)
        return 1.0;
    const double w = kTwoPi * frequencyHz / sampleRate;
    double m = 1.0;
    for (const auto& b : p.bands)
        if (b.enabled)
            m *= FilterDesign::designBand (b, sampleRate).magnitude (w);
    return m;
}

} // namespace skjald
