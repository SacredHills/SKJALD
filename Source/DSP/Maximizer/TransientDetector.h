#pragma once

namespace skjald
{
/** Linked transient (attack) detector at the base rate.

    Two peak envelopes share a release time but differ in attack time: at a drum onset the
    fast envelope jumps while the slow one lags, so their ratio in dB marks the attack
    portion. The output is an "attackness" value in [0, 1] per sample (0 = sustained
    material, 1 = >= kFullScaleDb of fast-over-slow rise).

    The Maximizer uses it for transient-selective clipping (Transient Emphasis and the Punch
    algorithm): only the tips of attacks are soft-clipped (oversampled) instead of being
    gain-reduced over the whole lookahead window, which keeps the attack energy relative
    to the sustain (higher transient-to-body ratio), while sustained material such as bass
    is handled by clean gain reduction only. */
class TransientDetector
{
public:
    static constexpr double kFullScaleDb = 6.0;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Reads the input (not modified) and writes n attackness values. */
    void process (const float* const* in, int numChannels, int numSamples, float* attackness) noexcept;

private:
    double fastAtt = 0.0, slowAtt = 0.0, rel = 0.0;
    double fast = 0.0, slow = 0.0;
};

} // namespace skjald
