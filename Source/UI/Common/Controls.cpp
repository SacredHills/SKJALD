#include "Controls.h"

namespace skjald::ui
{
// ---- Knob -----------------------------------------------------------------------------
Knob::Knob (APVTS& state, const juce::String& paramID, const juce::String& n, bool bipolar, juce::Colour accentColour)
    : name (n)
{
    param = state.getParameter (paramID);
    jassert (param != nullptr);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (260);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", (juce::int64) accentColour.getARGB());
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setTooltip (param->getName (64) + "  (double-click to reset, shift-drag for fine control, click the number to type a value)");
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (compact ? 14 : 18);
    r.removeFromBottom (compact ? 16 : 20);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

juce::Rectangle<int> Knob::valueArea() const
{
    return getLocalBounds().removeFromBottom (compact ? 16 : 20);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (valueArea().contains (e.getPosition()))
        showValueEditor();
}

// Click the number under a knob to type a value (Enter applies, Esc cancels). Understands "-2", "-2.5 dB", "1.2k", "70ms".
void Knob::showValueEditor()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (font (compact ? 12.5f : 14.0f));
    editor->setColour (juce::TextEditor::backgroundColourId, colours::background);
    editor->setColour (juce::TextEditor::textColourId, colours::text);
    editor->setColour (juce::TextEditor::outlineColourId, colours::accent);
    editor->setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    editor->setBounds (valueArea());
    editor->setText (param->getCurrentValueAsText(), false);
    auto close = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Knob> (this)] { if (safe != nullptr) safe->editor.reset(); }); };
    editor->onReturnKey = [this, close]
    {
        auto t = editor->getText().trim().toLowerCase();
        double v = t.retainCharacters ("-+.0123456789").getDoubleValue();
        if (t.containsChar ('k') && ! t.containsIgnoreCase ("ms"))
            v *= 1000.0;
        const auto lo = slider.getMinimum(), hi = slider.getMaximum();
        if (t.containsAnyOf ("0123456789"))
            slider.setValue (juce::jlimit (lo, hi, v), juce::sendNotificationSync);
        close();
    };
    editor->onEscapeKey = close;
    editor->onFocusLost = close;
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
    editor->selectAll();
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (font (compact ? 11.5f : 12.5f, true));
    g.drawText (name.toUpperCase(), r.removeFromTop (compact ? 14 : 18), juce::Justification::centred, false);

    g.setColour (slider.isMouseOverOrDragging() ? colours::accent : colours::text);
    g.setFont (font (compact ? 12.5f : 14.0f));
    g.drawText (param->getCurrentValueAsText(), r.removeFromBottom (compact ? 16 : 20), juce::Justification::centred, false);
}

// ---- ParamToggle ------------------------------------------------------------------------
ParamToggle::ParamToggle (APVTS& state, const juce::String& paramID, const juce::String& buttonText, juce::Colour accentColour)
    : juce::TextButton (buttonText)
{
    setClickingTogglesState (true);
    getProperties().set ("accent", (juce::int64) accentColour.getARGB());
    if (auto* p = state.getParameter (paramID))
        setTooltip (p->getName (64));
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, *this);
}

// ---- ChoiceSelector --------------------------------------------------------------------
ChoiceSelector::ChoiceSelector (APVTS& state, const juce::String& paramID, juce::StringArray labels, bool v)
    : vertical (v)
{
    auto* param = state.getParameter (paramID);
    jassert (param != nullptr);

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->getProperties().set ("bold", true);
        b->onClick = [this, i]
        {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float value) { update ((int) std::lround (value)); }, nullptr);
    attachment->sendInitialUpdate();
}

void ChoiceSelector::update (int index)
{
    current = index;
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    repaint();
}

void ChoiceSelector::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    if (n == 0)
        return;
    if (vertical)
    {
        const int gap = 6;
        const int h = (r.getHeight() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
        {
            auto br = r.removeFromTop (h);
            r.removeFromTop (gap);
            buttons[i]->setBounds (descriptions.isEmpty() ? br : br.removeFromTop (juce::jmin (br.getHeight(), 30)));
        }
    }
    else
    {
        const int gap = 4;
        const int w = (r.getWidth() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
        {
            buttons[i]->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (gap);
        }
    }
}

void ChoiceSelector::paint (juce::Graphics& g)
{
    if (! vertical || descriptions.isEmpty())
        return;
    for (int i = 0; i < buttons.size() && i < descriptions.size(); ++i)
    {
        const auto b = buttons[i]->getBounds();
        const auto area = juce::Rectangle<int> (b.getX() + 4, b.getBottom() + 1, b.getWidth() - 8, 16);
        g.setColour (i == current ? colours::textDim : colours::textFaint);
        g.setFont (font (11.0f));
        g.drawText (descriptions[i], area, juce::Justification::centredLeft, true);
    }
}

// ---- ParamCombo ------------------------------------------------------------------------
ParamCombo::ParamCombo (APVTS& state, const juce::String& paramID)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
        addItemList (p->choices, 1);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, *this);
}

} // namespace skjald::ui
