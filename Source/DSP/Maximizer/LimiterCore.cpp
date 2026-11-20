#include "LimiterCore.h"
#include "SoftClipper.h"
#include "../Utilities/MathUtils.h"

#include <cmath>
#include <algorithm>

namespace skjald
{
void LimiterCore::prepare (double baseRate, int oversamplingFactor, int maxLookaheadBaseSamples, int coreDelayBaseSamples,
                           int maxBlockOversampled)
{
    factor = std::max (1, oversamplingFactor);
    osRate = baseRate * factor;
    maxWindow = std::max (1, maxLookaheadBaseSamples * factor);
    signalDelay = std::max (1, coreDelayBaseSamples * factor);
    controlInterval = 8 * factor; // control updates every 8 base-rate samples

    // True-peak detection: >= 8x of the base rate in total. At 8x/16x the oversampled
    // samples themselves are used (worst-case under-read 0.14 dB at 0.45 fs, which the
    // output guard corrects); below that an interpolating detector runs at the OS rate.
    // Sample-peak mode uses the same latency, so toggling true peak never misaligns.
    const int detFactor = 8 / std::min (8, factor);
    useDetector = detFactor > 1;
    ringSize = signalDelay + 2;
    for (auto& c : ch)
    {
        c.signal.prepare (signalDelay);
        c.reqRing.assign ((size_t) ringSize, 1.0f);
        c.hold.prepare (maxWindow);
        for (auto& b : c.boxes)
            b.prepare (maxWindow);
        // At 1x the detector must be accurate up to fs/2. At 2x/4x the content near the
        // oversampled Nyquist is removed by decimation anyway (the output guard verifies the
        // final result), so a wide, cheap transition band is sufficient.
        if (useDetector)
            c.detector.prepare (detFactor, 60.0, factor == 1 ? 0.515 : 0.65);
        c.peaks.assign ((size_t) std::max (1, maxBlockOversampled), 0.0f);
    }
    detectorLatency = useDetector ? ch[0].detector.getLatency() : 0;
    for (auto& c : ch)
        c.samplePeakAlign.prepare (std::max (1, detectorLatency));
    ceilingAlign.prepare (signalDelay);

    clipMix.reset (osRate, 0.02);
    clipLevelDb.reset (osRate, 0.05);

    const double ctrlRate = osRate / controlInterval;
    avgCoeff = onePoleCoeff (0.4, ctrlRate);
    reset();
}

void LimiterCore::reset() noexcept
{
    for (auto& c : ch)
    {
        c.signal.reset();
        std::fill (c.reqRing.begin(), c.reqRing.end(), 1.0f); // unity history: no fade-in
        c.reqPos = 0;
        c.hold.reset();
        for (auto& b : c.boxes)
            b.reset (1.0f);
        if (useDetector)
            c.detector.reset();
        c.samplePeakAlign.reset();
        c.release1 = c.release2 = c.lastHeld = 1.0f;
    }
    ceilingAlign.reset (1.0f);
    controlCounter = 0;
    intervalMinEnv = intervalMinHeld = 1.0f;
    demandAverageDb = 0.0;
    blockMinGain = blockMeanGain = 1.0f;
    firstBlock = true;
}

void LimiterCore::computeTargets (const CoreSettings& s) noexcept
{
    const int lookahead = (int) std::lround (s.lookaheadMs * 0.001 * osRate);
    targetWindow = std::clamp ((int) std::lround ((double) lookahead * (double) s.attackFraction), 1, maxWindow);
    targetWindow = std::min (targetWindow, signalDelay + 1 - detectorLatency);

    // Split the window into `stages` boxes whose combined support is exactly the window:
    // sum(len_i) - (stages - 1) == window
    const int stages = std::clamp (s.attackStages, 1, 3);
    const int total = targetWindow + stages - 1;
    for (int i = 0; i < 3; ++i)
        targetBoxLen[i] = i < stages ? std::max (1, total / stages + (i < total % stages ? 1 : 0)) : 1;

    if (firstBlock)
    {
        // jump straight to the targets on the first block after a reset
        window = targetWindow;
        for (int i = 0; i < 3; ++i)
            boxLen[i] = targetBoxLen[i];
        for (auto& c : ch)
        {
            c.hold.setLength (window);
            for (int i = 0; i < 3; ++i)
            {
                c.boxes[(size_t) i].setLength (boxLen[i]);
                c.boxes[(size_t) i].reset (1.0f);
            }
        }
        requiredDelay = std::max (0, signalDelay - (window - 1) - detectorLatency);
        clipMix.setCurrentAndTarget (s.clipEnabled ? 1.0f : 0.0f);
        clipLevelDb.setCurrentAndTarget (s.clipAboveCeilingDb);
        firstBlock = false;
    }
    else
    {
        clipMix.setTarget (s.clipEnabled ? 1.0f : 0.0f);
        clipLevelDb.setTarget (s.clipAboveCeilingDb);
    }
}

void LimiterCore::updateControl (const CoreSettings& s) noexcept
{
    // Program-dependent release. "Demand" is the gain reduction the signal actually asks
    // for (held minimum), averaged over 400 ms; "envelope" is the gain reduction currently
    // applied. A short burst leaves a large envelope GR over a small average demand: recover
    // fast. Sustained demand keeps both equal: recover slowly (less modulation distortion
    // on dense low-frequency material, no pumping).
    const double envGrDb = -20.0 * std::log10 (std::max (1.0e-6f, intervalMinEnv));
    const double demandDb = -20.0 * std::log10 (std::max (1.0e-6f, intervalMinHeld));
    demandAverageDb = demandDb + (demandAverageDb - demandDb) * avgCoeff;

    double factorRel = 1.0;
    if (s.adaptiveRelease)
    {
        const double spike = clamp01 ((envGrDb - demandAverageDb) / 3.0);
        const double sustain = clamp01 (demandAverageDb / 3.0);
        const double slow = lerp (1.0, (double) s.adaptiveSlowFactor, sustain);
        factorRel = lerp (slow, (double) s.adaptiveFastFactor, spike);
    }

    const double tau = std::clamp ((double) s.releaseMs * factorRel, 2.0, 5000.0) * 0.001;
    // two cascaded stages are each shorter so the overall recovery time is comparable
    releaseCoeff1 = (float) onePoleCoeff (s.releaseStages >= 2 ? tau * 0.6 : tau, osRate);
    releaseCoeff2 = releaseCoeff1;

    intervalMinEnv = intervalMinHeld = 1.0f;
}

void LimiterCore::process (float* const* io, int numChannels, int numSamples, const CoreSettings& s, const float* ceiling,
                           const float* transientClip) noexcept
{
    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    numSamples = std::min (numSamples, (int) ch[0].peaks.size());
    computeTargets (s);

    const float link = clamp01 (s.link);
    const bool useKnee = s.kneeDb > 0.01f;
    const float kneeRatio = useKnee ? dbToGain (-s.kneeDb) : 1.0f;
    const bool twoStageRelease = s.releaseStages >= 2;

    // Pass 1: clipper (in place, crossfaded when switched) and detection.
    const bool clipActive = clipMix.isSmoothing() || clipMix.getTarget() > 0.0f;
    if (clipActive)
    {
        float clipRatio = dbToGain (clipLevelDb.getCurrent());
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = clipMix.next();
            if (clipLevelDb.isSmoothing())
                clipRatio = dbToGain (clipLevelDb.next());
            const float level = ceiling[i] * clipRatio;
            for (int c = 0; c < numChannels; ++c)
            {
                const float x = io[c][i];
                io[c][i] = x + (softClip (x, level, s.clipKnee) - x) * mix;
            }
        }
    }
    else
    {
        clipLevelDb.skip (numSamples);
    }

    if (transientClip != nullptr)
    {
        const float ratio = dbToGain (s.transientClipAboveCeilingDb);
        for (int i = 0; i < numSamples; ++i)
        {
            const float w = transientClip[i];
            if (w <= 0.0f)
                continue;
            const float level = ceiling[i] * ratio;
            for (int c = 0; c < numChannels; ++c)
            {
                const float x = io[c][i];
                io[c][i] = x + (softClip (x, level, s.transientClipKnee) - x) * w;
            }
        }
    }

    for (int c = 0; c < numChannels; ++c)
    {
        const float* x = io[c];
        auto& st = ch[(size_t) c];
        float* det = st.peaks.data();
        if (useDetector)
        {
            st.detector.process (x, det, numSamples);
            for (int i = 0; i < numSamples; ++i)
            {
                const float aligned = std::abs (st.samplePeakAlign.process (x[i], detectorLatency));
                // true peak never reads below the sample peak; sample-peak mode uses the
                // aligned sample so both modes share one latency
                det[i] = s.truePeakDetection ? std::max (det[i], aligned) : aligned;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                det[i] = std::abs (x[i]);
        }
    }

    float minGain = 1.0f;
    double gainSum = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        if (controlCounter == 0)
            updateControl (s);
        if (++controlCounter >= controlInterval)
            controlCounter = 0;

        // Slew the attack window (and the boxes) one sample at a time
        const int oldWindow = window;
        if (window < targetWindow) ++window;
        else if (window > targetWindow) --window;
        const int newDelay = std::max (0, signalDelay - (window - 1) - detectorLatency);
        const bool boxesMoving = boxLen[0] != targetBoxLen[0] || boxLen[1] != targetBoxLen[1] || boxLen[2] != targetBoxLen[2];
        if (boxesMoving)
            for (int k = 0; k < 3; ++k)
                boxLen[k] += (targetBoxLen[k] > boxLen[k]) - (targetBoxLen[k] < boxLen[k]);

        // Required gain per channel (detector values refer to the sample detectorLatency ago)
        const float ceil = ceiling[i];
        const float kneeStart = ceil * kneeRatio;
        float req[kMaxChannels] { 1.0f, 1.0f };
        for (int c = 0; c < numChannels; ++c)
        {
            const float det = ch[(size_t) c].peaks[(size_t) i];
            float r = 1.0f;
            if (det > kneeStart)
            {
                if (! useKnee)
                    r = ceil / det;
                else
                {
                    const float over = gainToDb (det / ceil);
                    float gr;
                    if (over >= s.kneeDb)
                        gr = over;
                    else
                    {
                        const float k = over + s.kneeDb;
                        gr = k * k / (4.0f * s.kneeDb);
                    }
                    r = dbToGain (-gr);
                }
            }
            req[c] = r;
        }

        if (numChannels == 2)
        {
            const float m = std::min (req[0], req[1]);
            req[0] += (m - req[0]) * link;
            req[1] += (m - req[1]) * link;
        }

        const float clampLevel = ceilingAlign.process (ceil, signalDelay - detectorLatency);

        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];

            // write the newest required gain into the ring
            st.reqRing[(size_t) st.reqPos] = req[c];
            auto readAt = [&st, this] (int delay)
            {
                int idx = st.reqPos - delay;
                if (idx < 0)
                    idx += ringSize;
                return st.reqRing[(size_t) idx];
            };

            if (window != oldWindow)
                st.hold.setLength (window);

            float held;
            if (newDelay == requiredDelay)
                held = st.hold.process (readAt (newDelay));
            else if (newDelay < requiredDelay)
            {
                // window grew: also push the value that the shorter read delay would skip
                st.hold.process (readAt (requiredDelay));
                held = st.hold.process (readAt (newDelay));
            }
            else
            {
                // window shrank: that value was already pushed; keep the (conservative) hold
                held = st.lastHeld;
            }
            st.lastHeld = held;
            intervalMinHeld = std::min (intervalMinHeld, held);
            st.reqPos = (st.reqPos + 1 == ringSize) ? 0 : st.reqPos + 1;

            // release: instant attack, one-pole recovery (optionally two cascaded stages).
            // Each stage stays <= its input, so the envelope never exceeds the held minimum.
            st.release1 = held < st.release1 ? held : held + (st.release1 - held) * releaseCoeff1;
            st.release2 = st.release1 < st.release2 ? st.release1 : st.release1 + (st.release2 - st.release1) * releaseCoeff2;
            const float env = twoStageRelease ? st.release2 : st.release1;
            intervalMinEnv = std::min (intervalMinEnv, env);

            if (boxesMoving)
                for (int k = 0; k < 3; ++k)
                    st.boxes[(size_t) k].stepTowards (boxLen[k]);

            float g = st.boxes[0].process (env);
            g = st.boxes[1].process (g);
            g = st.boxes[2].process (g);

            const float delayed = st.signal.process (io[c][i], signalDelay);
            io[c][i] = std::clamp (delayed * g, -clampLevel, clampLevel); // numerical safety net

            minGain = std::min (minGain, g);
            gainSum += g;
        }
        requiredDelay = newDelay;
    }

    blockMinGain = minGain;
    blockMeanGain = numSamples > 0 ? (float) (gainSum / (numSamples * numChannels)) : 1.0f;
}

} // namespace skjald
