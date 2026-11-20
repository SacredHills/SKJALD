#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../UI/CustomLookAndFeel.h"
#include "../UI/MainWindow.h"

namespace skjald
{
/** Resizable editor: the whole interface is laid out at a fixed design size and scaled
    uniformly, so text, knobs, graphs and meters always keep their proportions. */
class SkjaldAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SkjaldAudioProcessorEditor (SkjaldAudioProcessor&);
    ~SkjaldAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SkjaldAudioProcessor& processor;
    ui::SkjaldLookAndFeel lookAndFeel;
    std::unique_ptr<ui::MainWindow> main;
    juce::TooltipWindow tooltips { this, 700 };
    double lastTime = 0.0;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkjaldAudioProcessorEditor)
};

} // namespace skjald
