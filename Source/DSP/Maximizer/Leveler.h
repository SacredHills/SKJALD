#pragma once

#include "LimiterModes.h"

namespace skjald
{
/** Slow RMS compressor in front of the peak limiter (base rate). Its job is the
    sustained density of the master, so its time constants are tens to hundreds of ms
    and it never has to catch peaks. Stereo-linked per the link amount. */
class Leveler
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** attackScale > 1 slows the attack (used by Transient Emphasis to let hits through). */
    void process (float* const* io, int numChannels, int numSamples, const LevelerConfig& cfg,
                  float ceilingDb, float link, float attackScale) noexcept;

    /** Current gain reduction in dB (positive number). */
    float getGainReductionDb() const noexcept { return currentGrDb; }

private:
    double fs = 48000.0;
    double msCoeff = 0.0;
    double ms[2] { 0.0, 0.0 };
    double grDb[2] { 0.0, 0.0 };
    float currentGrDb = 0.0f;
};

} // namespace skjald
