#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "AudioTaps.h"
#include "ABComparison.h"
#include "../Presets/PresetManager.h"
#include "Engine/MasteringEngine.h"

namespace skjald
{
class SkjaldAudioProcessor : public juce::AudioProcessor
{
public:
    SkjaldAudioProcessor();
    ~SkjaldAudioProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // SKJALD
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    MasteringEngine& getEngine() noexcept { return engine; }
    ProcessorTaps& getTaps() noexcept { return taps; }
    ABComparison& getAB() noexcept { return ab; }
    PresetManager& getPresets() noexcept { return presets; }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

    /** Editor size and selected tab persist with the session. */
    juce::ValueTree& getUIState() noexcept { return uiState; }

private:
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>& buffer, bool forceBypass) noexcept;

    juce::AudioProcessorValueTreeState state;
    params::ParameterReader reader;
    MasteringEngine engine;
    ProcessorTaps taps;
    ABComparison ab;
    PresetManager presets;
    juce::ValueTree uiState { "UI" };

    juce::AudioBuffer<float> conversionBuffer; // for 64-bit host buffers
    std::atomic<double> currentSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkjaldAudioProcessor)
};

} // namespace skjald
