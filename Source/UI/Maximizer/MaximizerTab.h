#pragma once

#include "../Common/Controls.h"
#include "../MeterComponents/Meters.h"
#include "../PresetBrowser.h"
#include "../../Plugin/AudioTaps.h"

#include <array>

namespace skjald::ui
{
/** Scrolling gain-reduction history: peak GR as a filled area hanging from 0 dB, average
    GR as a line, auto-ranged dB scale, current value read-out. */
class GRHistoryDisplay : public juce::Component
{
public:
    explicit GRHistoryDisplay (SkjaldAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void refresh (double dt);

private:
    static constexpr int kColumns = 720;
    static constexpr double kHistorySeconds = 6.0;
    struct Column { float peak = 0.0f, avg = 0.0f, inPeak = 0.0f, outPeak = 0.0f; };

    SkjaldAudioProcessor& processor;
    std::array<Column, kColumns> columns {};
    int writePos = 0;
    double accumulated = 0.0, avgSum = 0.0;
    float colIn = 0.0f, colOut = 0.0f;
    float colPeak = 0.0f, currentPeak = 0.0f, displayPeak = 0.0f, rangeDb = 6.0f;
    std::array<GainReductionPoint, 4096> scratch {};
};

/** Loudness and level read-outs of the Maximizer tab. */
class LoudnessPanel : public juce::Component
{
public:
    explicit LoudnessPanel (SkjaldAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (double dt);
    void mouseDown (const juce::MouseEvent&) override;

private:
    SkjaldAudioProcessor& processor;
    LevelMeter grMeter;
};

class MaximizerTab : public juce::Component
{
public:
    explicit MaximizerTab (SkjaldAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh (double dt);

private:
    SkjaldAudioProcessor& processor;
    GRHistoryDisplay grDisplay;
    LoudnessPanel loudness;
    ChoiceSelector modeSelector;
    Knob inputGain, threshold, ceiling, character, release, stereoLink, transient, lookahead;
    ParamToggle thresholdLink;
    juce::TextButton learnButton { "LEARN" };
    juce::Label targetLabel;
    juce::Slider targetSlider;
    std::unique_ptr<APVTS::SliderAttachment> targetAttachment;
    bool learning = false;
    float lastThreshold = 0.0f;
    ParamToggle autoRelease, truePeak, bypass, delta, adaptive, softClip, autoGain;
    ChoiceSelector oversampling, linkMode;
    juce::TextButton advancedButton { "ADVANCED" };
    PresetBrowser presetBrowser;
    bool advancedOpen = false;
    juce::Rectangle<int> advancedArea, knobArea, modeArea;
};

} // namespace skjald::ui
