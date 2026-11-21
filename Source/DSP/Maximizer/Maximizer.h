#pragma once

#include "LimiterModes.h"
#include "LimiterCore.h"
#include "Leveler.h"
#include "TruePeakGuard.h"
#include "TransientDetector.h"
#include "../Oversampling/Oversampler.h"
#include "../Utilities/DelayLine.h"
#include "../Utilities/Smoothers.h"
#include "../../Metering/KWeighting.h"

#include <array>
#include <vector>

namespace skjald
{
struct MaximizerParams
{
    float inputGainDb = 0.0f;      // -24 .. +24
    float thresholdDb = 0.0f;      // -30 .. 0   (drive = input gain - threshold)
    float ceilingDb = -1.0f;       // -12 .. 0   (dBTP when true peak is on)
    float releaseMs = 100.0f;      // 5 .. 1000
    bool autoRelease = false;
    float character = 5.0f;        // 0 (fast) .. 10 (slow): overall attack and release speed, 5 = neutral
    float lookaheadMs = 5.0f;      // 0 .. 10
    float stereoLink = 1.0f;       // 0 .. 1
    float transientEmphasis = 0.0f; // 0 .. 1
    LimiterMode mode = LimiterMode::Modern;
    bool truePeak = true;
    int oversamplingIndex = 2;     // 0..4 -> 1x, 2x, 4x, 8x, 16x
    bool adaptiveRelease = true;
    bool softClip = false;
    StereoLinkMode linkMode = StereoLinkMode::Standard;
    bool autoGain = false;
    bool delta = false;
    bool bypass = false;
};

/** Mastering maximizer. Signal flow (per channel, linked control):

      input -> drive -> leveler (RMS, base rate) [+ transient detection]
            -> [upsample -> lookahead peak core (+ bounded soft clip after the gain) -> downsample]  (oversampled path)
            -> latency pad -> true-peak guard -> ceiling clamp -> AGC -> out

    Total latency is constant for every setting (all oversampling paths are padded to the
    longest, lookahead uses a fixed 10 ms delay), so changing any parameter never changes
    the latency reported to the host.

    Changing the oversampling factor starts the new path in parallel, lets it fill its
    delay lines, then crossfades (no gap, no click). */
class Maximizer
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kNumPaths = 5;
    static constexpr double kMaxLookaheadMs = 10.0;
    static constexpr float kTruePeakCoreMarginDb = 0.03f;
    static constexpr float kArcFastShare = 0.4f, kArcBaseAttackMs = 60.0f, kArcBaseReleaseMs = 300.0f;
    static constexpr float kTruePeakGuardMarginDb = 0.01f;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const MaximizerParams& p) noexcept;

    /** In place; numSamples <= maxBlockSize. */
    void process (float* const* io, int numChannels, int numSamples) noexcept;

    int getLatency() const noexcept { return latency; }

    /** Gain reduction of the last block in dB (positive): peak and average. */
    float getPeakGainReductionDb() const noexcept { return lastPeakGrDb; }
    float getAverageGainReductionDb() const noexcept { return lastAvgGrDb; }
    float getAutoGainDb() const noexcept { return agcAppliedDb; }
    int getActiveOversamplingFactor() const noexcept { return 1 << activePath; }

    static int oversamplingIndexToFactor (int idx) noexcept { return 1 << idx; }

private:
    struct Path
    {
        Oversampler os;
        LimiterCore core;
        std::array<DelayLine<float>, kMaxChannels> pad;
        int padDelay = 0;
    };

    void runPath (Path& path, float* const* in, float* const* out, int numChannels, int n, const CoreSettings& cs,
                  const float* ceilingBase, const float* transientBase, const float* levelerGrBase) noexcept;
    CoreSettings buildCoreSettings() const noexcept;

    double fs = 48000.0;
    int maxBlock = 0, latency = 0, osRegionLatency = 0;
    MaximizerParams params {};
    ModeConfig modeCfg {};

    std::array<Path, kNumPaths> paths;
    int activePath = 2, pendingPath = -1, warmupRemaining = 0;
    float pathFade = 0.0f, pathFadeStep = 0.001f;

    Leveler leveler;
    TransientDetector transientDetector;
    TruePeakGuard guard;

    std::array<DelayLine<float>, kMaxChannels> dryDelay, deltaDelay;
    DelayLine<float> ceilingToGuard, ceilingToOutput;  // ceiling stream aligned with the audio
    std::vector<float> ceilingBuf, ceilingOsBuf, guardCeilingBuf, transientBuf, transientOsBuf, levelerGrBuf, levelerGrOsBuf;
    LinearSmoother transientAmount;
    LinearSmoother driveDb, ceilingDb, wetMix, deltaMix;

    // Automatic gain compensation (K-weighted, slow, attenuation only)
    std::array<KWeightingFilter, kMaxChannels> agcInFilter, agcOutFilter;
    double agcMsIn = 0.0, agcMsOut = 0.0, agcCoeff = 0.0;
    DecibelGainSmoother agcGain;
    float agcAppliedDb = 0.0f;

    std::array<std::vector<float>, kMaxChannels> dryBuf, drivenBuf, pathBufA, pathBufB;

    float lastPeakGrDb = 0.0f, lastAvgGrDb = 0.0f;
    bool firstParams = true;
};

} // namespace skjald
