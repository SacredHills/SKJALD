#pragma once

#include "EQTypes.h"
#include "../Filters/Biquad.h"
#include "../Utilities/Smoothers.h"

#include <array>

namespace skjald
{
/** One EQ band: a cascade of up to four biquads per channel.

    Continuous parameters (frequency, gain, Q) are smoothed in perceptual domains
    (log-frequency, dB, log-Q) and the coefficients are recomputed every
    kCoeffUpdateInterval samples while a ramp is running.

    Structural changes (filter type, slope, enable) cannot be smoothed through the
    coefficients, so the band crossfades from the old cascade to the new one. */
class EQBand
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kCoeffUpdateInterval = 16;

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Called once per processing block from the audio thread. */
    void setParameters (const EQBandParams& p) noexcept;

    void process (double* const* channels, int numChannels, int numSamples) noexcept;

    bool isActive() const noexcept { return active.cascade.numSections > 0 || fading; }

private:
    struct Chain
    {
        SectionCascade cascade;
        std::array<std::array<BiquadState, kMaxSectionsPerBand>, kMaxChannels> state {};

        void resetState() noexcept
        {
            for (auto& ch : state)
                for (auto& s : ch)
                    s.reset();
        }

        inline double run (int ch, double x) noexcept
        {
            auto& st = state[(size_t) ch];
            for (int i = 0; i < cascade.numSections; ++i)
                x = st[(size_t) i].process (cascade.sections[(size_t) i], x);
            return x;
        }
    };

    void redesignFromSmoothed() noexcept;
    EQBandParams smoothedParams() const noexcept;

    double fs = 48000.0;
    EQBandParams target {};
    bool hasTarget = false;

    LinearSmoother logFreq, gainDb, logQ;

    Chain active, old;
    bool fading = false;
    double fadePos = 0.0, fadeStep = 0.0;
};

} // namespace skjald
