#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace skjald
{
SkjaldAudioProcessor::SkjaldAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SKJALD", params::createLayout()),
      reader (state),
      ab (*this),
      presets (state)
{
    engine.setTap (&taps);
    // Report a realistic latency before the host prepares the plug-in (updated in prepareToPlay)
    engine.prepare (48000.0, 2);
    setLatencySamples (engine.getLatency());
    uiState.setProperty ("width", 1180, nullptr);
    uiState.setProperty ("height", 720, nullptr);
    uiState.setProperty ("tab", 0, nullptr);
}

SkjaldAudioProcessor::~SkjaldAudioProcessor()
{
    engine.setTap (nullptr);
}

bool SkjaldAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SkjaldAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int channels = juce::jlimit (1, 2, getTotalNumOutputChannels());
    engine.prepare (sampleRate, channels);
    engine.setParameters (reader.read());
    conversionBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    // Constant for every parameter setting (see Maximizer), so it never changes while playing
    setLatencySamples (engine.getLatency());
}

void SkjaldAudioProcessor::releaseResources() {}

double SkjaldAudioProcessor::getTailLengthSeconds() const
{
    // The latency is reported separately; the release of the limiter is not a tail.
    return 0.0;
}

juce::AudioProcessorParameter* SkjaldAudioProcessor::getBypassParameter() const
{
    // Host bypass drives the plug-in's own latency-compensated, crossfaded bypass
    return state.getParameter (params::globalBypass);
}

template <typename SampleType>
void SkjaldAudioProcessor::processInternal (juce::AudioBuffer<SampleType>& buffer, bool forceBypass) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int channels = juce::jlimit (1, 2, juce::jmin (numOut, buffer.getNumChannels()));
    if (channels <= 0 || buffer.getNumSamples() == 0)
        return;

    auto p = reader.read();
    if (forceBypass)
        p.globalBypass = true;
    engine.setParameters (p);

    if constexpr (std::is_same_v<SampleType, float>)
    {
        float* ptrs[2] { buffer.getWritePointer (0), channels > 1 ? buffer.getWritePointer (1) : nullptr };
        engine.process (ptrs, channels, buffer.getNumSamples());
    }
    else
    {
        // 64-bit host buffers: converted through a preallocated 32-bit buffer in chunks
        const int chunk = conversionBuffer.getNumSamples();
        for (int start = 0; start < buffer.getNumSamples(); start += chunk)
        {
            const int n = juce::jmin (chunk, buffer.getNumSamples() - start);
            float* ptrs[2] { conversionBuffer.getWritePointer (0), conversionBuffer.getWritePointer (1) };
            for (int c = 0; c < channels; ++c)
            {
                const double* src = buffer.getReadPointer (c, start);
                for (int i = 0; i < n; ++i)
                    ptrs[c][i] = (float) src[i];
            }
            engine.process (ptrs, channels, n);
            for (int c = 0; c < channels; ++c)
            {
                double* dst = buffer.getWritePointer (c, start);
                for (int i = 0; i < n; ++i)
                    dst[i] = (double) ptrs[c][i];
            }
        }
    }
}

void SkjaldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processInternal (buffer, false); }
void SkjaldAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processInternal (buffer, false); }
void SkjaldAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processInternal (buffer, true); }
void SkjaldAudioProcessor::processBlockBypassed (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processInternal (buffer, true); }

juce::AudioProcessorEditor* SkjaldAudioProcessor::createEditor()
{
    return new SkjaldAudioProcessorEditor (*this);
}

void SkjaldAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("SKJALD_STATE");
    root.setProperty ("version", JucePlugin_VersionString, nullptr);
    root.appendChild (state.copyState(), nullptr);
    root.appendChild (ab.toValueTree(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    juce::ValueTree names ("PRESETNAMES");
    for (int c = 0; c < 3; ++c)
        names.setProperty ("c" + juce::String (c), presets.getCurrentName ((PresetCategory) c), nullptr);
    root.appendChild (names, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void SkjaldAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("SKJALD_STATE"))
        return;

    const auto params = root.getChildWithName (state.state.getType());
    if (params.isValid())
        state.replaceState (params);
    ab.fromValueTree (root.getChildWithName ("AB"));
    const auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
        uiState.copyPropertiesFrom (ui, nullptr);
    const auto names = root.getChildWithName ("PRESETNAMES");
    for (int c = 0; c < 3; ++c)
        presets.setCurrentName ((PresetCategory) c, names.getProperty ("c" + juce::String (c)).toString());
}

} // namespace skjald

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new skjald::SkjaldAudioProcessor();
}
