#include "Maximizer.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void Maximizer::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = std::max (1, maxBlockSize);

    const int maxLookaheadBase = (int) std::ceil (kMaxLookaheadMs * 0.001 * fs);
    const int coreDelayBase = maxLookaheadBase + LimiterCore::kMaxDetectorLatency + 1;

    int maxOsLatency = 0;
    for (int i = 0; i < kNumPaths; ++i)
    {
        auto& p = paths[(size_t) i];
        p.os.prepare (i, maxBlock);
        p.core.prepare (fs, 1 << i, maxLookaheadBase, coreDelayBase, maxBlock << i);
        maxOsLatency = std::max (maxOsLatency, p.os.getLatency());
    }
    for (auto& p : paths)
    {
        p.padDelay = maxOsLatency - p.os.getLatency();
        for (auto& d : p.pad)
            d.prepare (std::max (1, p.padDelay));
    }

    leveler.prepare (fs);
    transientDetector.prepare (fs);
    guard.prepare (fs, maxBlock);

    osRegionLatency = maxOsLatency + coreDelayBase;
    latency = osRegionLatency + guard.getLatency();

    for (int c = 0; c < kMaxChannels; ++c)
    {
        dryDelay[(size_t) c].prepare (latency);
        deltaDelay[(size_t) c].prepare (latency);
        dryBuf[(size_t) c].assign ((size_t) maxBlock, 0.0f);
        drivenBuf[(size_t) c].assign ((size_t) maxBlock, 0.0f);
        pathBufA[(size_t) c].assign ((size_t) maxBlock, 0.0f);
        pathBufB[(size_t) c].assign ((size_t) maxBlock, 0.0f);
        agcInFilter[(size_t) c].prepare (fs);
        agcOutFilter[(size_t) c].prepare (fs);
    }
    ceilingToGuard.prepare (osRegionLatency);
    ceilingToOutput.prepare (guard.getLatency());
    ceilingBuf.assign ((size_t) maxBlock, 1.0f);
    guardCeilingBuf.assign ((size_t) maxBlock, 1.0f);
    ceilingOsBuf.assign ((size_t) (maxBlock << (kNumPaths - 1)), 1.0f);
    transientBuf.assign ((size_t) maxBlock, 0.0f);
    transientOsBuf.assign ((size_t) (maxBlock << (kNumPaths - 1)), 0.0f);
    levelerGrBuf.assign ((size_t) maxBlock, 0.0f);
    levelerGrOsBuf.assign ((size_t) (maxBlock << (kNumPaths - 1)), 0.0f);
    transientAmount.reset (fs, 0.03);

    driveDb.reset (fs, 0.03);
    ceilingDb.reset (fs, 0.03);
    wetMix.reset (fs, 0.02);
    deltaMix.reset (fs, 0.02);
    agcGain.reset (fs, 0.3);
    agcCoeff = onePoleCoeff (3.0, fs);
    pathFadeStep = (float) (1.0 / std::max (1.0, 0.01 * fs));

    firstParams = true;
    reset();
}

void Maximizer::reset() noexcept
{
    for (auto& p : paths)
    {
        p.os.reset();
        p.core.reset();
        for (auto& d : p.pad)
            d.reset();
    }
    leveler.reset();
    transientDetector.reset();
    guard.reset();
    for (auto& d : dryDelay) d.reset();
    for (auto& d : deltaDelay) d.reset();
    ceilingToGuard.reset (dbToGain (params.ceilingDb));
    ceilingToOutput.reset (dbToGain (params.ceilingDb));
    for (auto& f : agcInFilter) f.reset();
    for (auto& f : agcOutFilter) f.reset();
    agcMsIn = agcMsOut = 0.0;
    agcGain.setCurrentAndTargetDb (0.0f);
    agcAppliedDb = 0.0f;
    pendingPath = -1;
    warmupRemaining = 0;
    pathFade = 0.0f;
    lastPeakGrDb = lastAvgGrDb = 0.0f;
}

void Maximizer::setParameters (const MaximizerParams& p) noexcept
{
    params = p;
    modeCfg = getModeConfig (p.mode);
    const int target = std::clamp (p.oversamplingIndex, 0, kNumPaths - 1);

    if (firstParams)
    {
        firstParams = false;
        driveDb.setCurrentAndTarget (p.inputGainDb - p.thresholdDb);
        ceilingDb.setCurrentAndTarget (p.ceilingDb);
        wetMix.setCurrentAndTarget (p.bypass ? 0.0f : 1.0f);
        deltaMix.setCurrentAndTarget (p.delta ? 1.0f : 0.0f);
        transientAmount.setCurrentAndTarget (clamp01 (modeCfg.transientClip + clamp01 (p.transientEmphasis) * (1.0f - modeCfg.transientClip)));
        activePath = target;
        pendingPath = -1;
        ceilingToGuard.reset (dbToGain (p.ceilingDb));
        ceilingToOutput.reset (dbToGain (p.ceilingDb));
        return;
    }

    driveDb.setTarget (p.inputGainDb - p.thresholdDb);
    ceilingDb.setTarget (p.ceilingDb);
    wetMix.setTarget (p.bypass ? 0.0f : 1.0f);
    deltaMix.setTarget (p.delta ? 1.0f : 0.0f);
    transientAmount.setTarget (clamp01 (modeCfg.transientClip + clamp01 (p.transientEmphasis) * (1.0f - modeCfg.transientClip)));

    if (pendingPath >= 0)
    {
        if (target == activePath)
            pendingPath = -1; // switched back before the crossfade: just cancel
        else if (target != pendingPath)
            pendingPath = -1; // restart below with the new target
    }
    if (pendingPath < 0 && target != activePath)
    {
        pendingPath = target;
        auto& np = paths[(size_t) pendingPath];
        np.os.reset();
        np.core.reset();
        for (auto& d : np.pad)
            d.reset();
        warmupRemaining = osRegionLatency;
        pathFade = 0.0f;
    }
}

CoreSettings Maximizer::buildCoreSettings() const noexcept
{
    CoreSettings s;
    const float e = clamp01 (params.transientEmphasis);
    s.lookaheadMs = std::clamp (params.lookaheadMs, 0.0f, (float) kMaxLookaheadMs);
    // Character: overall speed, like Ozone's Fast..Slow slider. 5 is neutral; each step is 23 %
    // (0 -> 0.35x times, 10 -> 2.8x times). Release scales fully, the attack window with the square root.
    const float speed = std::pow (2.0f, (std::clamp (params.character, 0.0f, 10.0f) - 5.0f) * 0.3f);
    s.attackFraction = std::clamp (modeCfg.attackFraction * (1.0f - 0.6f * e) * std::sqrt (speed), 0.05f, 1.0f);
    s.attackStages = modeCfg.attackStages;
    s.kneeDb = modeCfg.kneeDb;
    s.releaseStages = modeCfg.releaseStages;
    s.releaseMs = std::clamp ((params.autoRelease ? modeCfg.autoReleaseMs : params.releaseMs) * speed, 1.0f, 5000.0f);
    if (params.autoRelease)
    {
        // ARC-style auto release: a fast envelope for peaks above the sustained reduction and a slow base
        // envelope that follows the average reduction (see CoreSettings)
        s.releaseMs = std::clamp (modeCfg.autoReleaseMs * kArcFastShare * speed, 2.0f, 5000.0f);
        s.arcBaseAttackMs = kArcBaseAttackMs * std::sqrt (speed);
        s.arcBaseReleaseMs = kArcBaseReleaseMs * modeCfg.releaseMultiplier * speed;
    }
    s.adaptiveRelease = params.adaptiveRelease || params.autoRelease;
    s.adaptiveSlowFactor = modeCfg.adaptiveSlowFactor;
    s.adaptiveFastFactor = modeCfg.adaptiveFastFactor;
    s.link = clamp01 (params.stereoLink); // peak stage always follows the knob (Split only changes the leveler)
    // Bounded soft clipping is always on: every mode shaves its few overshoots with its own depth
    // (Transparent / Modern / Smooth / Punch 0.7-1 dB, Aggressive 0.25 dB). The old Soft Clip switch is gone.
    const float depth = modeCfg.clipper.depthDb;
    s.clipDepthDb = depth;
    s.clipKnee = modeCfg.clipper.knee;
    s.transientDepthDb = modeCfg.transientDepthDb;
    s.lfReleaseFactor = modeCfg.lfReleaseFactor;
    s.lfReleaseFloorMs = modeCfg.lfReleaseFloorMs;
    s.truePeakDetection = params.truePeak;
    return s;
}

void Maximizer::runPath (Path& path, float* const* in, float* const* out, int numChannels, int n, const CoreSettings& cs,
                         const float* ceilingBase, const float* transientBase, const float* levelerGrBase) noexcept
{
    const int F = path.os.getFactor();
    // Ceiling at the oversampled rate (sample-and-hold), including the true-peak core margin
    const float margin = params.truePeak ? dbToGain (-kTruePeakCoreMarginDb) : 1.0f;
    float* ceilOs = ceilingOsBuf.data();
    for (int i = 0; i < n; ++i)
        std::fill (ceilOs + i * F, ceilOs + (i + 1) * F, ceilingBase[i] * margin);

    float* transOs = nullptr;
    if (transientBase != nullptr)
    {
        transOs = transientOsBuf.data();
        for (int i = 0; i < n; ++i)
            std::fill (transOs + i * F, transOs + (i + 1) * F, transientBase[i]);
    }

    float* grOs = levelerGrOsBuf.data();
    for (int i = 0; i < n; ++i)
        std::fill (grOs + i * F, grOs + (i + 1) * F, levelerGrBase[i]);

    path.os.upsample (in, numChannels, n);
    float* osPtrs[kMaxChannels] { path.os.getOversampledChannel (0), path.os.getOversampledChannel (1) };
    path.core.process (osPtrs, numChannels, n * F, cs, ceilOs, transOs, grOs);
    path.os.downsample (out, numChannels, n);
    if (path.padDelay > 0)
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                out[c][i] = path.pad[(size_t) c].process (out[c][i], path.padDelay);
}

void Maximizer::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    const int n = std::min (numSamples, maxBlock);

    float* dry[kMaxChannels] { dryBuf[0].data(), dryBuf[1].data() };
    float* driven[kMaxChannels] { drivenBuf[0].data(), drivenBuf[1].data() };
    float* bufA[kMaxChannels] { pathBufA[0].data(), pathBufA[1].data() };
    float* bufB[kMaxChannels] { pathBufB[0].data(), pathBufB[1].data() };

    // 1. keep the dry input; apply drive
    for (int c = 0; c < numChannels; ++c)
        std::copy (io[c], io[c] + n, dry[c]);

    if (driveDb.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = dbToGain (driveDb.next());
            for (int c = 0; c < numChannels; ++c)
                driven[c][i] = dry[c][i] * g;
        }
    }
    else
    {
        const float g = dbToGain (driveDb.getCurrent());
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < n; ++i)
                driven[c][i] = dry[c][i] * g;
    }

    // Per-sample ceiling stream (input time); delayed copies follow the audio through the chain
    float* ceil = ceilingBuf.data();
    if (ceilingDb.isSmoothing())
        for (int i = 0; i < n; ++i)
            ceil[i] = dbToGain (ceilingDb.next());
    else
        std::fill (ceil, ceil + n, dbToGain (ceilingDb.getCurrent()));
    const float ceilDb = ceilingDb.getCurrent();

    // 2. leveler (base rate) writes into bufA, the driven signal stays as delta reference
    for (int c = 0; c < numChannels; ++c)
        std::copy (driven[c], driven[c] + n, bufA[c]);
    const float levelerLink = params.linkMode == StereoLinkMode::Split ? 1.0f : clamp01 (params.stereoLink);
    leveler.process (bufA, numChannels, n, modeCfg.leveler, ceilDb, levelerLink, 1.0f + 2.0f * clamp01 (params.transientEmphasis),
                     levelerGrBuf.data());
    // transient-selective clipping weights (Transient Emphasis / Punch)
    const float* transient = nullptr;
    {
        float* tw = transientBuf.data();
        const float* lev[kMaxChannels] { bufA[0], bufA[1] };
        transientDetector.process (lev, numChannels, n, tw);
        if (transientAmount.isSmoothing() || transientAmount.getTarget() > 0.0f)
        {
            for (int i = 0; i < n; ++i)
                tw[i] *= transientAmount.next();
            transient = tw;
        }
    }

    // 3. oversampled limiter path(s)
    const CoreSettings cs = buildCoreSettings();
    if (pendingPath >= 0)
    {
        // run the pending path on a copy of the same input
        for (int c = 0; c < numChannels; ++c)
            std::copy (bufA[c], bufA[c] + n, bufB[c]);
        runPath (paths[(size_t) activePath], bufA, bufA, numChannels, n, cs, ceil, transient, levelerGrBuf.data());
        runPath (paths[(size_t) pendingPath], bufB, bufB, numChannels, n, cs, ceil, transient, levelerGrBuf.data());

        if (warmupRemaining > 0)
        {
            warmupRemaining -= n;
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                pathFade = std::min (1.0f, pathFade + pathFadeStep);
                for (int c = 0; c < numChannels; ++c)
                    bufA[c][i] += (bufB[c][i] - bufA[c][i]) * pathFade;
            }
            if (pathFade >= 1.0f)
            {
                activePath = pendingPath;
                pendingPath = -1;
            }
        }
    }
    else
    {
        runPath (paths[(size_t) activePath], bufA, bufA, numChannels, n, cs, ceil, transient, levelerGrBuf.data());
    }

    // 4. output verification and final clamp
    const float guardMargin = params.truePeak ? dbToGain (-kTruePeakGuardMarginDb) : 1.0f;
    float* guardCeil = guardCeilingBuf.data();
    for (int i = 0; i < n; ++i)
        guardCeil[i] = ceilingToGuard.process (ceil[i], osRegionLatency);
    for (int i = 0; i < n; ++i)
        ceil[i] = guardCeil[i] * guardMargin; // reuse: guard target
    guard.process (bufA, numChannels, n, ceil, params.truePeak);
    for (int i = 0; i < n; ++i)
    {
        const float outCeil = ceilingToOutput.process (guardCeil[i], guard.getLatency());
        for (int c = 0; c < numChannels; ++c)
            bufA[c][i] = std::clamp (bufA[c][i], -outCeil, outCeil);
    }

    // 5. automatic gain compensation (measure input and limited output, attenuation only)
    {
        double sIn = 0.0, sOut = 0.0;
        for (int c = 0; c < numChannels; ++c)
        {
            auto& fi = agcInFilter[(size_t) c];
            auto& fo = agcOutFilter[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                const double a = fi.process (dry[c][i]);
                const double b = fo.process (bufA[c][i]);
                sIn += a * a;
                sOut += b * b;
            }
        }
        // block-wise one-pole (3 s) on the K-weighted mean square summed over channels
        const double blockCoeff = std::pow (agcCoeff, (double) n);
        agcMsIn = sIn / n + (agcMsIn - sIn / n) * blockCoeff;
        agcMsOut = sOut / n + (agcMsOut - sOut / n) * blockCoeff;
    }
    float agcTargetDb = 0.0f;
    if (params.autoGain && agcMsIn > 1.0e-8 && agcMsOut > 1.0e-12)
        agcTargetDb = (float) std::clamp (10.0 * std::log10 (agcMsIn / agcMsOut), -40.0, 0.0);
    agcGain.setTargetDb (params.autoGain ? agcTargetDb : 0.0f);

    // 6. delta / AGC / bypass mix
    const bool mixing = wetMix.isSmoothing() || deltaMix.isSmoothing() || agcGain.isSmoothing()
                        || wetMix.getTarget() < 1.0f || deltaMix.getTarget() > 0.0f
                        || agcGain.getCurrentGain() != 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float w = wetMix.next();
        const float d = deltaMix.next();
        const float a = agcGain.next();
        for (int c = 0; c < numChannels; ++c)
        {
            const float dryAligned = dryDelay[(size_t) c].process (dry[c][i], latency);
            const float refAligned = deltaDelay[(size_t) c].process (driven[c][i], latency);
            if (! mixing)
            {
                io[c][i] = bufA[c][i];
                continue;
            }
            const float limited = bufA[c][i];
            const float wet = limited * a + ((refAligned - limited) - limited * a) * d;
            io[c][i] = dryAligned + (wet - dryAligned) * w;
        }
    }
    agcAppliedDb = gainToDb (agcGain.getCurrentGain());

    // 7. metering
    const auto& core = paths[(size_t) activePath].core;
    const float levGr = leveler.getGainReductionDb();
    lastPeakGrDb = levGr - gainToDb (core.getMinGain()) - gainToDb (guard.getMinGain());
    lastAvgGrDb = levGr - gainToDb (core.getMeanGain());
    if (params.bypass)
        lastPeakGrDb = lastAvgGrDb = 0.0f;
}

} // namespace skjald
