#pragma once

#include "../DSP/Oversampling/TruePeakDetector.h"
#include <vector>

#include <array>
#include <atomic>

namespace skjald
{
/** ITU-R BS.1770-4 Annex 2 true-peak meter. The oversampling factor is chosen so the
    reconstruction runs at >= 352.8 kHz (8x at 44.1/48 kHz, 4x at 88.2/96 kHz, 2x at
    176.4/192 kHz) using the halfband-cascade TruePeakDetector. That exceeds the
    standard's 4x / 48-tap example filter: under-reading stays below ~0.1 dB up to 0.47 fs. */
class TruePeakMeter
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate);
    void reset() noexcept; // not thread-safe: call from prepare / non-realtime code only
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Thread-safe request to clear the max-peak hold (applied on the next process call). */
    void requestMaxReset() noexcept { resetRequest.store (true, std::memory_order_release); }

    /** Highest true peak since the last reset (linear). */
    float getMaxPeak() const noexcept { return maxPeak.load (std::memory_order_relaxed); }

    /** Highest true peak of the most recent process() call (linear), per channel. */
    float getBlockPeak (int ch) const noexcept { return blockPeak[(size_t) ch].load (std::memory_order_relaxed); }

    int getFactor() const noexcept { return detectors[0].getFactor(); }

private:
    std::array<TruePeakDetector, kMaxChannels> detectors;
    std::vector<float> scratch;
    std::atomic<float> maxPeak { 0.0f };
    std::array<std::atomic<float>, kMaxChannels> blockPeak {};
    std::atomic<bool> resetRequest { false };
};

/** Sample-peak meter with the same publishing model. */
class SamplePeakMeter
{
public:
    static constexpr int kMaxChannels = 2;

    void reset() noexcept
    {
        maxPeak = 0.0f;
        for (auto& b : blockPeak)
            b = 0.0f;
    }

    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    float getMaxPeak() const noexcept { return maxPeak.load (std::memory_order_relaxed); }
    float getBlockPeak (int ch) const noexcept { return blockPeak[(size_t) ch].load (std::memory_order_relaxed); }

private:
    std::atomic<float> maxPeak { 0.0f };
    std::array<std::atomic<float>, kMaxChannels> blockPeak {};
};

} // namespace skjald
