#pragma once

namespace skjald
{
enum class LimiterMode : int
{
    Transparent = 0,
    Modern,
    Aggressive,
    Punch,
    Smooth
};

constexpr int kNumLimiterModes = 5;

enum class StereoLinkMode : int
{
    Standard = 0, // one link amount for the whole limiter
    Split         // sustained (leveler) stage fully linked, peak stage uses the link knob
};

/** Slow RMS "leveler" stage that runs in front of the peak limiter. It shapes the
    sustained density of the master; the peak stage only catches what is left. */
struct LevelerConfig
{
    bool enabled = false;
    float thresholdBelowCeilingDb = 10.0f; // RMS threshold relative to the ceiling
    float ratio = 1.5f;
    float attackMs = 20.0f;
    float releaseMs = 250.0f;
    float maxGainReductionDb = 3.0f;
    float kneeDb = 6.0f;
};

/** Pre-limiter clipper (runs oversampled). Level is relative to the ceiling. */
struct ClipperConfig
{
    bool builtIn = false;          // always active for this mode
    float levelAboveCeilingDb = 1.5f;
    float knee = 0.7f;             // fraction of the clip level where saturation starts
};

/** Everything that makes one limiting algorithm structurally different from another. */
struct ModeConfig
{
    // Peak stage
    int attackStages = 2;          // cascaded moving averages: 1 = linear ramp, 3 = near-Gaussian
    float attackFraction = 1.0f;   // share of the lookahead used as attack window
    float kneeDb = 0.0f;           // soft knee in the peak gain computer
    int releaseStages = 1;         // 2 = second-order (S-shaped) recovery
    float releaseMultiplier = 1.0f;
    float autoReleaseMs = 100.0f;  // release time used when Release = Auto
    float transientClip = 0.0f;          // built-in transient-selective clipping amount (0..1)
    float adaptiveSlowFactor = 1.6f;     // release stretch for sustained gain reduction
    float adaptiveFastFactor = 0.35f;    // release shrink for short peaks

    LevelerConfig leveler;
    ClipperConfig clipper;
};

inline ModeConfig getModeConfig (LimiterMode mode) noexcept
{
    ModeConfig c;
    switch (mode)
    {
        case LimiterMode::Transparent:
            c.attackStages = 2;
            c.attackFraction = 1.0f;
            c.releaseStages = 2;
            c.releaseMultiplier = 1.0f;
            c.autoReleaseMs = 150.0f;
            c.adaptiveSlowFactor = 1.8f;
            c.adaptiveFastFactor = 0.4f;
            c.leveler.enabled = false;
            c.clipper = { false, 1.5f, 0.8f };
            break;

        case LimiterMode::Modern:
            c.attackStages = 2;
            c.attackFraction = 0.75f;
            c.releaseStages = 1;
            c.releaseMultiplier = 0.8f;
            c.autoReleaseMs = 100.0f;
            c.leveler = { true, 9.0f, 1.5f, 20.0f, 250.0f, 3.0f, 6.0f };
            c.clipper = { false, 1.0f, 0.7f };
            break;

        case LimiterMode::Aggressive:
            c.attackStages = 1;
            c.attackFraction = 0.35f;
            c.releaseStages = 1;
            c.releaseMultiplier = 0.5f;
            c.autoReleaseMs = 50.0f;
            c.adaptiveSlowFactor = 1.3f;
            c.adaptiveFastFactor = 0.3f;
            c.leveler = { true, 11.0f, 2.0f, 10.0f, 150.0f, 4.5f, 6.0f };
            c.clipper = { true, 1.0f, 0.6f };
            break;

        case LimiterMode::Punch:
            c.attackStages = 2;
            c.attackFraction = 0.3f;
            c.releaseStages = 1;
            c.releaseMultiplier = 0.9f;
            c.autoReleaseMs = 80.0f;
            c.transientClip = 0.6f;
            c.leveler = { true, 10.0f, 1.7f, 45.0f, 120.0f, 3.5f, 6.0f };
            c.clipper = { false, 1.0f, 0.75f };
            break;

        case LimiterMode::Smooth:
            c.attackStages = 3;
            c.attackFraction = 1.0f;
            c.kneeDb = 2.0f;
            c.releaseStages = 2;
            c.releaseMultiplier = 1.5f;
            c.autoReleaseMs = 300.0f;
            c.adaptiveSlowFactor = 2.0f;
            c.adaptiveFastFactor = 0.5f;
            c.leveler = { true, 6.0f, 1.25f, 50.0f, 500.0f, 1.5f, 8.0f };
            c.clipper = { false, 2.5f, 0.85f };
            break;
    }
    return c;
}

inline const char* getModeName (LimiterMode m) noexcept
{
    switch (m)
    {
        case LimiterMode::Transparent: return "Transparent";
        case LimiterMode::Modern:      return "Modern";
        case LimiterMode::Aggressive:  return "Aggressive";
        case LimiterMode::Punch:       return "Punch";
        case LimiterMode::Smooth:      return "Smooth";
    }
    return "";
}

} // namespace skjald
