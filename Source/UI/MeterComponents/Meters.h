#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace skjald
{
class SkjaldAudioProcessor;
}

namespace skjald::ui
{
/** Vertical peak meter with smooth ballistics (instant rise, 20 dB/s fall) and a 1.5 s
    peak hold. Values are pushed by the owner on the GUI timer. */
class LevelMeter : public juce::Component
{
public:
    /** Linear peak level, or (reduction style) a positive gain reduction in dB. */
    void setLevel (float value, double dt);
    void paint (juce::Graphics&) override;
    void setReductionStyle (bool r) { reduction = r; }
    static float dbToPos (float db, float minDb);

    float minDb = -48.0f;

private:
    float displayDb = -100.0f, holdDb = -100.0f;
    double holdTime = 0.0;
    bool reduction = false;
};

/** The global meter section visible on both tabs: input / output peak meters, true peak,
    integrated and short-term loudness. Click a value to reset its hold. */
class MeterStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MeterStrip (SkjaldAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (double dt);

private:
    SkjaldAudioProcessor& processor;
    LevelMeter inL, inR, outL, outR;
    float inPeakHold = 0.0f, outPeakHold = 0.0f;
    juce::Rectangle<int> readoutArea;
};

} // namespace skjald::ui
