#include "MasteringEngine.h"
#include "../DSP/Utilities/DenormalGuard.h"

#include <algorithm>

namespace skjald
{
void MasteringEngine::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::clamp (numChannels, 1, kMaxChannels);
    eq.prepare (fs, kChunkSize);
    maximizer.prepare (fs, kChunkSize);
    for (int c = 0; c < kMaxChannels; ++c)
    {
        bypassDelay[(size_t) c].prepare (maximizer.getLatency());
        inputCopy[(size_t) c].assign ((size_t) kChunkSize, 0.0f);
    }
    processedMix.reset (fs, 0.02);
    processedMix.setCurrentAndTarget (1.0f);
    outputTruePeak.prepare (fs);
    outputLoudness.prepare (fs);
    inputLoudness.prepare (fs);
    outputRms.prepare (fs);
    inputRms.prepare (fs);
    firstParams = true;
    reset();
}

void MasteringEngine::reset() noexcept
{
    eq.reset();
    maximizer.reset();
    for (auto& d : bypassDelay)
        d.reset();
    inputPeak.reset();
    outputPeak.reset();
    outputTruePeak.reset();
    outputLoudness.reset();
    inputLoudness.reset();
    outputRms.reset();
    inputRms.reset();
}

void MasteringEngine::setParameters (const EngineParams& p) noexcept
{
    eq.setParameters (p.eq);
    maximizer.setParameters (p.max);
    if (firstParams)
        processedMix.setCurrentAndTarget (p.globalBypass ? 0.0f : 1.0f);
    else
        processedMix.setTarget (p.globalBypass ? 0.0f : 1.0f);
    firstParams = false;
}

void MasteringEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    ScopedFlushDenormals noDenormals;
    numChannels = std::clamp (numChannels, 1, kMaxChannels);

    for (int start = 0; start < numSamples; start += kChunkSize)
    {
        const int n = std::min (kChunkSize, numSamples - start);
        float* chunk[kMaxChannels] { io[0] + start, numChannels > 1 ? io[1] + start : nullptr };
        const float* chunkConst[kMaxChannels] { chunk[0], chunk[1] };

        inputPeak.process (chunkConst, numChannels, n);
        float inPk = 0.0f;
        if (tap != nullptr)
            for (int c = 0; c < numChannels; ++c)
                for (int i = 0; i < n; ++i)
                    inPk = std::max (inPk, std::abs (chunk[c][i]));
        inputLoudness.process (chunkConst, numChannels, n);
        inputRms.process (chunkConst, numChannels, n);
        for (int c = 0; c < numChannels; ++c)
            std::copy (chunk[c], chunk[c] + n, inputCopy[(size_t) c].begin());

        if (tap != nullptr)
            tap->pushPreEQ (chunkConst, numChannels, n);

        eq.process (chunk, numChannels, n);

        if (tap != nullptr)
            tap->pushPostEQ (chunkConst, numChannels, n);

        maximizer.process (chunk, numChannels, n);

        // Global bypass: latency-compensated crossfade to the untouched input
        const bool mixing = processedMix.isSmoothing() || processedMix.getTarget() < 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float m = processedMix.next();
            for (int c = 0; c < numChannels; ++c)
            {
                const float dry = bypassDelay[(size_t) c].process (inputCopy[(size_t) c][(size_t) i], maximizer.getLatency());
                if (mixing)
                    chunk[c][i] = dry + (chunk[c][i] - dry) * m;
            }
        }

        outputPeak.process (chunkConst, numChannels, n);
        outputTruePeak.process (chunkConst, numChannels, n);
        outputLoudness.process (chunkConst, numChannels, n);
        outputRms.process (chunkConst, numChannels, n);

        const float bypassed = processedMix.getCurrent();
        const float grP = maximizer.getPeakGainReductionDb() * bypassed;
        const float grA = maximizer.getAverageGainReductionDb() * bypassed;
        grPeakDb.store (grP, std::memory_order_relaxed);
        grAvgDb.store (grA, std::memory_order_relaxed);
        agcDb.store (maximizer.getAutoGainDb(), std::memory_order_relaxed);
        if (tap != nullptr)
        {
            float outPk = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                for (int i = 0; i < n; ++i)
                    outPk = std::max (outPk, std::abs (chunk[c][i]));
            tap->pushLevels (inPk, outPk);
            tap->pushGainReduction (grP, grA);
        }
    }
}

} // namespace skjald
