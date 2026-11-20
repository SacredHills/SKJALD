#include "CustomLookAndFeel.h"
#include "Common/Theme.h"

namespace skjald::ui
{
SkjaldLookAndFeel::SkjaldLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panelRaised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::AlertWindow::backgroundColourId, panelRaised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outlineStrong);
    setColour (juce::TextButton::buttonColourId, panelRaised);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
    setColour (juce::TooltipWindow::textColourId, text);
}

void SkjaldLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                          float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float trackWidth = juce::jmax (2.5f, radius * 0.11f);
    const float arcRadius = radius - trackWidth * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const auto accentColour = s.getProperties().contains ("accent")
                                  ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"])
                                  : accent;
    const bool enabled = s.isEnabled();

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar knobs grow from the centre)
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (enabled ? accentColour : textFaint);
    g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const float bodyRadius = arcRadius - trackWidth * 1.4f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    juce::ColourGradient grad (knobBody.brighter (0.25f), centre.x, body.getY(), knobBody.darker (0.35f), centre.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (body, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (body.reduced (1.0f), 1.0f);

    // pointer
    const auto tip = centre.getPointOnCircumference (bodyRadius * 0.82f, angle);
    const auto base = centre.getPointOnCircumference (bodyRadius * 0.30f, angle);
    g.setColour (enabled ? text : textFaint);
    g.drawLine ({ base, tip }, juce::jmax (1.8f, radius * 0.06f));
}

void SkjaldLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = juce::jmin (6.0f, r.getHeight() * 0.3f);
    const bool on = b.getToggleState();
    const auto accentColour = b.getProperties().contains ("accent")
                                  ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"])
                                  : accent;
    const bool flat = b.getProperties().getWithDefault ("flat", false);

    if (flat)
    {
        if (highlighted || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.08f : 0.04f));
            g.fillRoundedRectangle (r, corner);
        }
        if (on)
        {
            g.setColour (accentColour);
            g.fillRect (r.removeFromBottom (2.0f).reduced (r.getWidth() * 0.18f, 0.0f));
        }
        return;
    }

    auto fill = on ? accentColour.withAlpha (0.18f) : panelRaised;
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.brighter (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? accentColour.withAlpha (0.85f) : outline);
    g.drawRoundedRectangle (r, corner, 1.0f);
}

juce::Font SkjaldLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const float scale = (float) b.getProperties().getWithDefault ("fontScale", 0.42);
    return font (juce::jmin (15.0f, (float) h * scale), (bool) b.getProperties().getWithDefault ("bold", false));
}

void SkjaldLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    using namespace colours;
    const bool on = b.getToggleState();
    auto c = on ? text : (highlighted ? text.withAlpha (0.85f) : textDim);
    if (! b.isEnabled())
        c = textFaint;
    if (b.getProperties().getWithDefault ("flat", false) && on)
        c = text;
    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void SkjaldLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? panelRaised.brighter (0.06f) : panelRaised);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // chevron
    const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f);
    p.lineTo (cx, cy + 2.5f);
    p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (textDim);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font SkjaldLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (juce::jmin (14.0f, (float) box.getHeight() * 0.5f));
}

void SkjaldLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void SkjaldLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panelRaised);
    g.setColour (colours::outlineStrong);
    g.drawRect (0, 0, w, h, 1);
}

void SkjaldLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& textToDraw,
                                           const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*)
{
    using namespace colours;
    if (isSeparator)
    {
        g.setColour (outline);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }
    if (isTicked)
    {
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 10.0f, (float) r.getCentreY() }));
    }
    g.setColour (isActive ? text : textFaint);
    g.setFont (getPopupMenuFont());
    g.drawText (textToDraw, r.withTrimmedLeft (20).withTrimmedRight (20), juce::Justification::centredLeft, true);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (textDim);
        g.drawText (shortcutKeyText, r.withTrimmedRight (8), juce::Justification::centredRight, true);
    }
    if (hasSubMenu)
    {
        juce::Path p;
        const float x = (float) r.getRight() - 10.0f, y = (float) r.getCentreY();
        p.startNewSubPath (x - 2.0f, y - 4.0f);
        p.lineTo (x + 2.0f, y);
        p.lineTo (x - 2.0f, y + 4.0f);
        g.setColour (textDim);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

void SkjaldLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setColour (colours::textFaint);
    g.setFont (font (11.0f, true));
    g.drawText (name.toUpperCase(), area.reduced (10, 0), juce::Justification::bottomLeft, true);
}

juce::Font SkjaldLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void SkjaldLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (colours::panelRaised);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
    g.setColour (colours::outlineStrong);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> SkjaldLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (font (13.0f), tipText) + 20);
    const int h = 26;
    return juce::Rectangle<int> (pos.x + 12, pos.y + 16, w, h).constrainedWithin (parent);
}

void SkjaldLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
}

void SkjaldLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? colours::accent : colours::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}

void SkjaldLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
{
    g.fillAll (colours::panelRaised);
    g.setColour (colours::outlineStrong);
    g.drawRect (alert.getLocalBounds(), 1);
    layout.draw (g, textArea.toFloat());
}

} // namespace skjald::ui
