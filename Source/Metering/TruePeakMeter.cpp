#include "TruePeakMeter.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void TruePeakMeter::prepare (double sampleRate)
{
    int factor = 8;
    if (sampleRate > 150000.0)
        factor = 2;
    else if (sampleRate > 75000.0)
        factor = 4;
    for (auto& d : detectors)
        d.prepare (factor, 70.0);
    scratch.assign ((size_t) TruePeakDetector::kMaxBlock, 0.0f);
    reset();
}

void TruePeakMeter::reset() noexcept
{
    for (auto& d : detectors)
        d.reset();
    maxPeak = 0.0f;
    for (auto& b : blockPeak)
        b = 0.0f;
}

void TruePeakMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    float overall = resetRequest.exchange (false, std::memory_order_acq_rel) ? 0.0f
                                                                          : maxPeak.load (std::memory_order_relaxed);
    for (int c = 0; c < numChannels; ++c)
    {
        float m = 0.0f;
        auto& det = detectors[(size_t) c];
        for (int start = 0; start < numSamples; start += TruePeakDetector::kMaxBlock)
        {
            const int len = std::min (TruePeakDetector::kMaxBlock, numSamples - start);
            det.process (channels[c] + start, scratch.data(), len);
            for (int i = 0; i < len; ++i)
                m = std::max (m, scratch[(size_t) i]);
        }
        blockPeak[(size_t) c].store (m, std::memory_order_relaxed);
        overall = std::max (overall, m);
    }
    if (numChannels == 1)
        blockPeak[1].store (blockPeak[0].load (std::memory_order_relaxed), std::memory_order_relaxed);
    maxPeak.store (overall, std::memory_order_relaxed);
}

void SamplePeakMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    float overall = maxPeak.load (std::memory_order_relaxed);
    for (int c = 0; c < numChannels; ++c)
    {
        float m = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            m = std::max (m, std::abs (channels[c][i]));
        blockPeak[(size_t) c].store (m, std::memory_order_relaxed);
        overall = std::max (overall, m);
    }
    if (numChannels == 1)
        blockPeak[1].store (blockPeak[0].load (std::memory_order_relaxed), std::memory_order_relaxed);
    maxPeak.store (overall, std::memory_order_relaxed);
}

} // namespace skjald
