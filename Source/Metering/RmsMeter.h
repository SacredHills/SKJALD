#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace skjald
{
/** Plain RMS level meter (no K-weighting), in dBFS. A full-scale sine reads -3.01 dBFS.

    Power is the mean of the channel mean-squares (mono and stereo read alike for identical
    channels). Like the loudness meter it works on 100 ms sub-blocks:
    - Momentary: last 400 ms
    - Short-term: last 3 s
    - Integrated: average power since the last reset; sub-blocks below -70 dBFS are ignored so
      silence does not drag the value down (the same idea as the absolute gate of BS.1770).

    Real-time safe, results are published through atomics. */
class RmsMeter
{
public:
    static constexpr float kSilenceDb = -100.0f;
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate) noexcept
    {
        subBlockLength = std::max (1, (int) std::lround (sampleRate * 0.1));
        reset();
    }

    void reset() noexcept
    {
        subBlockPos = 0;
        subBlockSum = 0.0;
        subBlocks.fill (0.0);
        subBlockCount = subBlockWrite = 0;
        resetIntegrated();
        momentary.store (kSilenceDb);
        shortTerm.store (kSilenceDb);
    }

    void resetIntegrated() noexcept { integratedResetRequest.store (true, std::memory_order_relaxed); }

    void process (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (integratedResetRequest.exchange (false, std::memory_order_relaxed))
        {
            integratedEnergy = 0.0;
            integratedCount = 0;
            integrated.store (kSilenceDb, std::memory_order_relaxed);
        }
        numChannels = std::min (numChannels, kMaxChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            double sq = 0.0;
            for (int c = 0; c < numChannels; ++c)
                sq += (double) channels[c][i] * channels[c][i];
            subBlockSum += sq / numChannels;
            if (++subBlockPos >= subBlockLength)
                finishSubBlock();
        }
    }

    float getMomentary() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float getShortTerm() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }

    static float powerToDb (double p) noexcept { return p <= 1.0e-10 ? kSilenceDb : (float) (10.0 * std::log10 (p)); }

private:
    void finishSubBlock() noexcept
    {
        const double power = subBlockSum / subBlockPos;
        subBlockSum = 0.0;
        subBlockPos = 0;
        subBlocks[(size_t) subBlockWrite] = power;
        subBlockWrite = (subBlockWrite + 1) % (int) subBlocks.size();
        subBlockCount = std::min (subBlockCount + 1, (int) subBlocks.size());

        auto mean = [this] (int count)
        {
            count = std::min (count, subBlockCount);
            double s = 0.0;
            for (int k = 1; k <= count; ++k)
                s += subBlocks[(size_t) ((subBlockWrite - k + (int) subBlocks.size()) % (int) subBlocks.size())];
            return s / std::max (1, count);
        };
        momentary.store (powerToDb (mean (4)), std::memory_order_relaxed);
        shortTerm.store (powerToDb (mean (30)), std::memory_order_relaxed);

        if (power > 1.0e-7) // -70 dBFS
        {
            integratedEnergy += power;
            ++integratedCount;
            integrated.store (powerToDb (integratedEnergy / (double) integratedCount), std::memory_order_relaxed);
        }
    }

    int subBlockLength = 4800, subBlockPos = 0;
    double subBlockSum = 0.0;
    std::array<double, 30> subBlocks {};
    int subBlockCount = 0, subBlockWrite = 0;
    double integratedEnergy = 0.0;
    uint64_t integratedCount = 0;
    std::atomic<float> momentary { kSilenceDb }, shortTerm { kSilenceDb }, integrated { kSilenceDb };
    std::atomic<bool> integratedResetRequest { false };
};
} // namespace skjald
