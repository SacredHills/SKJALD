#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace skjald::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Rotary knob bound to a parameter: name above, value text below, double-click resets,
    shift-drag / ctrl-drag for fine adjustment (JUCE velocity mode off, sensitivity reduced). */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramID, const juce::String& name, bool bipolar = false,
          juce::Colour accentColour = colours::accent);

    void resized() override;
    void paint (juce::Graphics&) override;
    void setCompact (bool c) { compact = c; resized(); }

    juce::Slider& getSlider() noexcept { return slider; }

private:
    class FineSlider : public juce::Slider
    {
    public:
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown() || e.mods.isCtrlDown();
            setMouseDragSensitivity (fine ? 1600 : 260);
            juce::Slider::mouseDrag (e);
        }
    };

    FineSlider slider;
    juce::String name;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    bool compact = false;
};

/** Toggle button bound to a boolean parameter (pill style). */
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (APVTS& state, const juce::String& paramID, const juce::String& buttonText,
                 juce::Colour accentColour = colours::accent);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Segmented selector bound to a choice parameter (one button per choice). */
class ChoiceSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSelector (APVTS& state, const juce::String& paramID, juce::StringArray labels, bool vertical = false);
    void resized() override;
    void setDescriptions (const juce::StringArray& d) { descriptions = d; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void update (int index);
    juce::OwnedArray<juce::TextButton> buttons;
    juce::StringArray descriptions;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool vertical;
    int current = 0;
};

/** Combo box bound to a choice parameter. */
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (APVTS& state, const juce::String& paramID);

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Section caption used across the interface. */
inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text,
                         juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (colours::textFaint);
    g.setFont (font (11.5f, true));
    g.drawText (text.toUpperCase(), area, j, true);
}

inline void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

} // namespace skjald::ui
