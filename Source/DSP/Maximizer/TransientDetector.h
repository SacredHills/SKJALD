#pragma once

namespace skjald
{
/** Linked transient (attack) detector at the base rate.

    A fast follower (0.5 ms) of the rectified, channel-linked signal is compared with a
    peak-held envelope (8 ms attack, 80 ms release). At an onset the fast follower jumps while
    the held envelope lags, so the ratio in dB marks the attack portion. A steady signal, bass
    included, never rises above its own recently held peak, so the ratio is <= 0 dB and the
    output is exactly 0; a dead zone covers the residual ripple. The output is an
    "attackness" value in [0, 1] per sample (held for ~4 ms after the rise) (1 = kDeadZoneDb + kFullScaleDb = 8 dB of rise).

    The Maximizer uses it for transient-selective clipping (Transient Emphasis and the Punch
    algorithm): the limiter is allowed to leave the tips of attacks a little above the
    ceiling, and they are then clipped (oversampled), instead of gain-reducing the whole
    lookahead window. That keeps attack energy relative to the sustain (higher
    transient-to-body ratio) while sustained material is handled by clean gain reduction. */
class TransientDetector
{
public:
    static constexpr double kFullScaleDb = 4.0;
    static constexpr double kDeadZoneDb = 4.0;
    static constexpr double kNoiseFloor = 0.02; // absolute floor (-34 dBFS) so that very low levels cannot flicker

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Reads the input (not modified) and writes n attackness values. */
    void process (const float* const* in, int numChannels, int numSamples, float* attackness) noexcept;

private:
    double fastCoeff = 0.0, holdAttack = 0.0, holdRelease = 0.0;
    double weightRelease = 0.0;
    double fast = 0.0, hold = 0.0, weight = 0.0;
};

} // namespace skjald
