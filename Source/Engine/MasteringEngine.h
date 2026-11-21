#pragma once

#include "../DSP/Equalizer/Equalizer.h"
#include "../DSP/Maximizer/Maximizer.h"
#include "../DSP/Utilities/DelayLine.h"
#include "../DSP/Utilities/Smoothers.h"
#include "../Metering/LoudnessMeter.h"
#include "../Metering/TruePeakMeter.h"

#include <array>
#include <atomic>
#include <vector>

namespace skjald
{
struct EngineParams
{
    EQParams eq;
    MaximizerParams max;
    bool globalBypass = false;
};

/** Optional realtime-safe observer (implementations must be lock-free / wait-free). */
struct EngineTap
{
    virtual ~EngineTap() = default;
    virtual void pushPreEQ (const float* const* channels, int numChannels, int numSamples) noexcept { (void) channels; (void) numChannels; (void) numSamples; }
    virtual void pushPostEQ (const float* const* channels, int numChannels, int numSamples) noexcept { (void) channels; (void) numChannels; (void) numSamples; }
    virtual void pushGainReduction (float peakDb, float averageDb) noexcept { (void) peakDb; (void) averageDb; }
    /** Sample peaks (linear, over all channels) of the chunk that entered and left the engine. */
    virtual void pushLevels (float inputPeak, float outputPeak) noexcept { (void) inputPeak; (void) outputPeak; }
};

/** Complete processing chain: INPUT -> EQUALIZER -> MAXIMIZER -> OUTPUT, plus metering.
    Pure C++ (no JUCE), shared by the plugin, the tests and the offline tool.
    Accepts any block size; works internally in chunks of kChunkSize samples. */
class MasteringEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kChunkSize = 256;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const EngineParams& p) noexcept;
    void process (float* const* io, int numChannels, int numSamples) noexcept;

    int getLatency() const noexcept { return maximizer.getLatency(); }
    double getSampleRate() const noexcept { return fs; }

    void setTap (EngineTap* t) noexcept { tap = t; }

    // Metering (thread-safe reads)
    SamplePeakMeter& getInputPeakMeter() noexcept { return inputPeak; }
    SamplePeakMeter& getOutputPeakMeter() noexcept { return outputPeak; }
    TruePeakMeter& getOutputTruePeakMeter() noexcept { return outputTruePeak; }
    LoudnessMeter& getOutputLoudness() noexcept { return outputLoudness; }
    LoudnessMeter& getInputLoudness() noexcept { return inputLoudness; }

    float getGainReductionDb() const noexcept { return grPeakDb.load (std::memory_order_relaxed); }
    float getAverageGainReductionDb() const noexcept { return grAvgDb.load (std::memory_order_relaxed); }
    float getAutoGainDb() const noexcept { return agcDb.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    Equalizer eq;
    Maximizer maximizer;
    std::array<DelayLine<float>, kMaxChannels> bypassDelay;
    std::array<std::vector<float>, kMaxChannels> inputCopy;
    LinearSmoother processedMix;

    SamplePeakMeter inputPeak, outputPeak;
    TruePeakMeter outputTruePeak;
    LoudnessMeter outputLoudness, inputLoudness;
    std::atomic<float> grPeakDb { 0.0f }, grAvgDb { 0.0f }, agcDb { 0.0f };

    EngineTap* tap = nullptr;
    bool firstParams = true;
};

} // namespace skjald
