#pragma once

#include "KWeighting.h"

#include <array>
#include <atomic>
#include <vector>
#include <cstdint>

namespace skjald
{
/** ITU-R BS.1770-4 / EBU R128 loudness meter (mono or stereo, channel weight 1.0).

    - Momentary: 400 ms sliding window, updated every 100 ms
    - Short-term: 3 s sliding window, updated every 100 ms
    - Integrated: 400 ms blocks with 75 % overlap, absolute gate -70 LUFS,
      relative gate -10 LU. Block energies are kept in a fixed 0.01 LU histogram
      (exact energy sums per bin), so memory is constant for unlimited durations and
      the audio thread never allocates.

    Results are published through atomics and may be read from any thread. */
class LoudnessMeter
{
public:
    static constexpr float kSilenceLufs = -100.0f;
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate);
    void reset() noexcept;            // everything
    void resetIntegrated() noexcept;  // integrated only

    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    float getMomentary() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float getShortTerm() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }

    static double energyToLufs (double e) noexcept;

private:
    void finishSubBlock() noexcept;
    void updateIntegrated() noexcept;

    static constexpr double kHistMin = -70.0, kHistMax = 10.0, kHistStep = 0.01;
    static constexpr int kHistBins = (int) ((kHistMax - kHistMin) / kHistStep) + 1;

    std::array<KWeightingFilter, kMaxChannels> filters;
    int subBlockLength = 4800, subBlockPos = 0;
    double subBlockSum = 0.0;
    std::array<double, 30> subBlocks {};
    int subBlockCount = 0, subBlockWrite = 0;

    std::vector<uint32_t> histCount;
    std::vector<double> histEnergy;
    double gatedTotalEnergy = 0.0;
    uint64_t gatedTotalCount = 0;

    std::atomic<float> momentary { kSilenceLufs }, shortTerm { kSilenceLufs }, integrated { kSilenceLufs };
    std::atomic<bool> integratedResetRequest { false };
};

} // namespace skjald
