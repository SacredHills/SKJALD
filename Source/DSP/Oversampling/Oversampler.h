#pragma once

#include "HalfbandStage.h"
#include "../Utilities/DelayLine.h"

#include <array>
#include <vector>

namespace skjald
{
/** Multi-stage 2^S oversampler (S = 0..4, i.e. 1x .. 16x) built from linear-phase
    halfband stages. The round-trip latency is padded at the top rate so it is always an
    integer number of base-rate samples (needed for sample-exact delta monitoring,
    bypass alignment and host latency reporting).

    Filter specification: passband to 0.4535 fs (20 kHz at 44.1 kHz), stopband from
    0.5465 fs (24.1 kHz at 44.1 kHz) with 100 dB attenuation for the first stage; the
    later stages only need to protect that band and are much shorter. */
class Oversampler
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxStages = 4;

    void prepare (int numStages, int maxBaseBlock, double attenuationDb = 100.0);
    void reset() noexcept;

    int getFactor() const noexcept { return 1 << stages; }
    int getNumStages() const noexcept { return stages; }

    /** Round-trip latency in base-rate samples (integer by construction). */
    int getLatency() const noexcept { return latencyBase; }

    /** Upsample n base samples per channel. Returns the oversampled buffer for channel ch,
        which the caller may process in place (n * factor samples). */
    void upsample (const float* const* in, int numChannels, int n) noexcept;
    float* getOversampledChannel (int ch) noexcept { return work[(size_t) ch][(size_t) stages].data(); }

    /** Downsample the (processed) oversampled buffers back to n base samples. */
    void downsample (float* const* out, int numChannels, int n) noexcept;

private:
    int stages = 0, maxBlock = 0, padTop = 0, latencyBase = 0;
    std::array<std::array<HalfbandStage, kMaxStages>, kMaxChannels> filters;
    // work[ch][s] holds the signal at rate fs * 2^s
    std::array<std::array<std::vector<float>, kMaxStages + 1>, kMaxChannels> work;
    std::array<DelayLine<float>, kMaxChannels> pad;
};

} // namespace skjald
