#pragma once

#include <juce_core/juce_core.h>
#include "Engine/MasteringEngine.h"

#include <array>
#include <vector>

namespace skjald
{
/** Single-producer / single-consumer lock-free sample FIFO (audio thread -> GUI).
    The audio thread never blocks: when the GUI is not reading, new samples are dropped. */
class SampleFifo
{
public:
    static constexpr int kSize = 1 << 15;

    SampleFifo() : buffer ((size_t) kSize, 0.0f) {}

    /** Audio thread: pushes the mono sum of the channels. */
    void pushMono (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        const auto scope = fifo.write (std::min (numSamples, fifo.getFreeSpace()));
        const float scale = numChannels > 1 ? 0.5f : 1.0f;
        auto copy = [&] (int start, int size, int offset)
        {
            for (int i = 0; i < size; ++i)
            {
                float v = channels[0][offset + i];
                if (numChannels > 1)
                    v += channels[1][offset + i];
                buffer[(size_t) (start + i)] = v * scale;
            }
        };
        if (scope.blockSize1 > 0) copy (scope.startIndex1, scope.blockSize1, 0);
        if (scope.blockSize2 > 0) copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    /** GUI thread: reads up to maxSamples, returns the number read. */
    int pull (float* dest, int maxSamples) noexcept
    {
        const auto scope = fifo.read (std::min (maxSamples, fifo.getNumReady()));
        if (scope.blockSize1 > 0) std::copy_n (buffer.data() + scope.startIndex1, scope.blockSize1, dest);
        if (scope.blockSize2 > 0) std::copy_n (buffer.data() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo fifo { kSize };
    std::vector<float> buffer;
};

struct GainReductionPoint
{
    float peakDb = 0.0f;
    float averageDb = 0.0f;
    int numSamples = 0;
    float inputPeak = 0.0f, outputPeak = 0.0f; // linear sample peaks of the same block (waveform display)
};

class GainReductionFifo
{
public:
    static constexpr int kSize = 8192;

    void push (const GainReductionPoint& p) noexcept
    {
        const auto scope = fifo.write (fifo.getFreeSpace() > 0 ? 1 : 0);
        if (scope.blockSize1 > 0) buffer[(size_t) scope.startIndex1] = p;
        else if (scope.blockSize2 > 0) buffer[(size_t) scope.startIndex2] = p;
    }

    int pull (GainReductionPoint* dest, int maxPoints) noexcept
    {
        const auto scope = fifo.read (std::min (maxPoints, fifo.getNumReady()));
        for (int i = 0; i < scope.blockSize1; ++i) dest[i] = buffer[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i) dest[scope.blockSize1 + i] = buffer[(size_t) (scope.startIndex2 + i)];
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo fifo { kSize };
    std::array<GainReductionPoint, kSize> buffer {};
};

/** Engine observer used by the plugin: spectrum (pre/post EQ) and gain-reduction history. */
class ProcessorTaps : public EngineTap
{
public:
    void pushPreEQ (const float* const* ch, int numCh, int n) noexcept override
    {
        if (analyzerActive.load (std::memory_order_relaxed))
            pre.pushMono (ch, numCh, n);
        lastBlock = n;
    }

    void pushPostEQ (const float* const* ch, int numCh, int n) noexcept override
    {
        if (analyzerActive.load (std::memory_order_relaxed))
            post.pushMono (ch, numCh, n);
    }

    void pushLevels (float inputPeak, float outputPeak) noexcept override
    {
        lastIn = inputPeak;
        lastOut = outputPeak;
    }

    void pushGainReduction (float peakDb, float avgDb) noexcept override
    {
        gr.push ({ peakDb, avgDb, lastBlock, lastIn, lastOut });
    }

    SampleFifo pre, post;
    GainReductionFifo gr;
    std::atomic<bool> analyzerActive { false };

private:
    int lastBlock = 0;
    float lastIn = 0.0f, lastOut = 0.0f;
};

} // namespace skjald
