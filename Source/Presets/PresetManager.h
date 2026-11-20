#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

namespace skjald
{
enum class PresetCategory
{
    Global = 0, // EQ + Maximizer
    EQ,
    Maximizer
};

struct Preset
{
    juce::String name;
    PresetCategory category = PresetCategory::Global;
    bool factory = true;
    std::map<juce::String, float> values; // parameter ID -> real (denormalised) value
    juce::File file;                      // user presets only
};

/** Factory presets (compiled in) and user presets (XML files in the user's application data
    folder). Loading a preset first resets every parameter of its category to the default,
    then applies the stored values, so a preset always produces the same complete state.
    Parameter changes go through the normal parameter path (smoothed by the DSP).
    Message thread only. */
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    juce::Array<Preset> getPresets (PresetCategory c) const;
    void load (const Preset& p);
    bool saveUserPreset (const juce::String& name, PresetCategory c);
    bool deleteUserPreset (const Preset& p);

    juce::String getCurrentName (PresetCategory c) const { return current[(int) c]; }
    void setCurrentName (PresetCategory c, const juce::String& n) { current[(int) c] = n; }

    static juce::File getUserDirectory (PresetCategory c);
    static juce::String categoryName (PresetCategory c);
    static juce::StringArray parameterIDs (PresetCategory c);
    static const juce::Array<Preset>& factoryPresets();

    static constexpr const char* fileExtension = ".skjaldpreset";

private:
    Preset readFile (const juce::File& f) const;
    juce::AudioProcessorValueTreeState& state;
    juce::String current[3];
};

} // namespace skjald
