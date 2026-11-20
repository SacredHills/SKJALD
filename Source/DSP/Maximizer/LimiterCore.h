#pragma once

#include "EnvelopeBlocks.h"
#include "../Oversampling/TruePeakDetector.h"
#include "../Utilities/DelayLine.h"
#include "../Utilities/Smoothers.h"

#include <array>
#include <vector>

namespace skjald
{
/** Per-block settings for the peak limiter core (all derived by the Maximizer). */
struct CoreSettings
{
    float lookaheadMs = 5.0f;
    float attackFraction = 1.0f;
    int attackStages = 2;
    float kneeDb = 0.0f;
    int releaseStages = 1;
    float releaseMs = 100.0f;
    bool adaptiveRelease = true;
    float adaptiveSlowFactor = 1.6f;
    float adaptiveFastFactor = 0.35f;
    float link = 1.0f;
    bool clipEnabled = false;
    float clipAboveCeilingDb = 1.5f;
    float clipKnee = 0.7f;
    bool truePeakDetection = true;
    // transient-selective clipping: level above the ceiling and soft knee
    float transientClipAboveCeilingDb = 0.3f;
    float transientClipKnee = 0.5f;
};

/** Feed-forward lookahead peak limiter that runs at the oversampled rate.

    Per sample and channel:
      1. optional soft clipper and transient-selective clipper (oversampled, so their
         harmonics are filtered on the way down)
      2. detector: |x|, or an interpolated reconstruction (>= 8x of the base rate in
         total) when true-peak detection is on
      3. required gain r = min(1, ceiling / peak), with optional soft knee
      4. stereo link (convex blend towards the minimum of both channels: never louder)
      5. sliding minimum over the attack window A (hold)
      6. program-dependent release (one or two cascaded one-pole stages)
      7. 1-3 cascaded moving averages whose total support is A (attack shape)
      8. gain applied to the input delayed by a fixed amount

    Every averaged value is a minimum over a window that contains the aligned sample, so the
    smoothed gain is <= the required gain when the delayed sample is output: the ceiling
    holds by construction. The final clamp (using the ceiling that was valid when the
    sample's gain was computed) is a numerical safety net only.

    The ceiling arrives as a per-sample stream so ceiling automation stays aligned with the
    audio through the internal delay. Attack-window changes are slewed one sample at a time
    without dropping any detector values, and the clipper is crossfaded when switched. */
class LimiterCore
{
public:
    static constexpr int kMaxChannels = 2;
    /** Upper bound of the detector latency in oversampled samples. */
    static constexpr int kMaxDetectorLatency = 96;

    void prepare (double baseRate, int oversamplingFactor, int maxLookaheadBaseSamples, int coreDelayBaseSamples,
                  int maxBlockOversampled);
    void reset() noexcept;

    /** ceiling: per-sample linear ceiling at the input time of each sample (numSamples values).
        transientClip: optional per-sample weight (0..1) of the transient-selective clipper. */
    void process (float* const* io, int numChannels, int numSamples, const CoreSettings& s, const float* ceiling,
                  const float* transientClip = nullptr) noexcept;

    float getMinGain() const noexcept { return blockMinGain; }
    float getMeanGain() const noexcept { return blockMeanGain; }

private:
    void computeTargets (const CoreSettings& s) noexcept;
    void updateControl (const CoreSettings& s) noexcept;

    double osRate = 48000.0;
    int factor = 1, signalDelay = 1, maxWindow = 1;
    int detectorLatency = 0;
    bool useDetector = false;
    int controlInterval = 8, controlCounter = 0;

    // window slewing state
    int window = 1, targetWindow = 1;
    int boxLen[3] { 1, 1, 1 }, targetBoxLen[3] { 1, 1, 1 };
    int requiredDelay = 0;

    struct Channel
    {
        DelayLine<float> signal;
        std::vector<float> reqRing; // required-gain history (peekable)
        int reqPos = 0;
        SlidingMin hold;
        std::array<BoxFilter, 3> boxes;
        TruePeakDetector detector;  // only used when the oversampling factor is < 8
        DelayLine<float> samplePeakAlign;
        std::vector<float> peaks;
        float release1 = 1.0f, release2 = 1.0f, lastHeld = 1.0f;
    };
    std::array<Channel, kMaxChannels> ch;
    int ringSize = 1;
    DelayLine<float> ceilingAlign;

    LinearSmoother clipMix, clipLevelDb;

    // control-rate state
    float releaseCoeff1 = 0.999f, releaseCoeff2 = 0.999f;
    float intervalMinEnv = 1.0f, intervalMinHeld = 1.0f;
    double demandAverageDb = 0.0, avgCoeff = 0.0;

    float blockMinGain = 1.0f, blockMeanGain = 1.0f;
    bool firstBlock = true;
};

} // namespace skjald
