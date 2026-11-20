#include "MainWindow.h"
#include "../Plugin/PluginProcessor.h"

namespace skjald::ui
{
// ---- TabNavigation ----------------------------------------------------------------------
TabNavigation::TabNavigation()
{
    for (auto* b : { &eqTab, &maxTab })
    {
        b->getProperties().set ("flat", true);
        b->getProperties().set ("bold", true);
        b->getProperties().set ("fontScale", 0.36);
        addAndMakeVisible (b);
    }
    eqTab.onClick = [this] { setCurrent (0); if (onChange) onChange (0); };
    maxTab.onClick = [this] { setCurrent (1); if (onChange) onChange (1); };
    setCurrent (0);
}

void TabNavigation::resized()
{
    auto r = getLocalBounds();
    eqTab.setBounds (r.removeFromLeft (r.getWidth() / 2));
    maxTab.setBounds (r);
}

void TabNavigation::setCurrent (int index)
{
    current = juce::jlimit (0, 1, index);
    eqTab.setToggleState (current == 0, juce::dontSendNotification);
    maxTab.setToggleState (current == 1, juce::dontSendNotification);
}

// ---- MainWindow -------------------------------------------------------------------------
MainWindow::MainWindow (SkjaldAudioProcessor& p)
    : processor (p), globalPresets (p, PresetCategory::Global),
      globalBypass (p.getState(), params::globalBypass, "BYPASS", colours::danger),
      meters (p), eqTab (p), maxTab (p)
{
    setSize (kDesignWidth, kDesignHeight);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &tabs, &globalPresets, &buttonA, &buttonB, &copyAB, &globalBypass, &meters })
        addAndMakeVisible (c);
    addChildComponent (eqTab);
    addChildComponent (maxTab);

    tabs.onChange = [this] (int i) { showTab (i); };
    buttonA.setTooltip ("Settings A");
    buttonB.setTooltip ("Settings B");
    copyAB.setTooltip ("Copy the active settings to the other slot");
    globalBypass.setTooltip ("Global bypass (latency compensated)");
    buttonA.onClick = [this] { processor.getAB().selectSlot (0); updateABButtons(); };
    buttonB.onClick = [this] { processor.getAB().selectSlot (1); updateABButtons(); };
    copyAB.onClick = [this] { processor.getAB().copyActiveToOther(); };
    for (auto* b : { &buttonA, &buttonB })
        b->getProperties().set ("bold", true);
    updateABButtons();

    showTab ((int) processor.getUIState().getProperty ("tab", 0));
}

void MainWindow::showTab (int index)
{
    tabs.setCurrent (index);
    eqTab.setVisible (index == 0);
    maxTab.setVisible (index == 1);
    processor.getUIState().setProperty ("tab", index, nullptr);
}

void MainWindow::updateABButtons()
{
    const int s = processor.getAB().getActiveSlot();
    buttonA.setToggleState (s == 0, juce::dontSendNotification);
    buttonB.setToggleState (s == 1, juce::dontSendNotification);
    copyAB.setButtonText (s == 0 ? "A>B" : "B>A");
}

void MainWindow::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (56);
    {
        auto h = headerArea.reduced (16, 10);
        h.removeFromLeft (150); // logo
        tabs.setBounds (h.removeFromLeft (290));
        globalBypass.setBounds (h.removeFromRight (84).reduced (0, 2));
        h.removeFromRight (14);
        copyAB.setBounds (h.removeFromRight (46).reduced (0, 2));
        h.removeFromRight (4);
        buttonB.setBounds (h.removeFromRight (34).reduced (0, 2));
        h.removeFromRight (4);
        buttonA.setBounds (h.removeFromRight (34).reduced (0, 2));
        h.removeFromRight (16);
        globalPresets.setBounds (h.removeFromRight (320).reduced (0, 2));
    }

    r.reduce (12, 10);
    meters.setBounds (r.removeFromRight (150));
    r.removeFromRight (10);
    eqTab.setBounds (r);
    maxTab.setBounds (r);
}

void MainWindow::paint (juce::Graphics& g)
{
    using namespace colours;
    g.fillAll (background);
    g.setColour (header);
    g.fillRect (headerArea);
    g.setColour (outline);
    g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) getWidth());

    // logo: a stylised shield mark + word mark (SKJALD = Norse poet)
    auto logo = headerArea.reduced (16, 12).removeFromLeft (150).toFloat();
    auto mark = logo.removeFromLeft (26.0f).withSizeKeepingCentre (22.0f, 26.0f);
    juce::Path shield;
    shield.startNewSubPath (mark.getX(), mark.getY());
    shield.lineTo (mark.getRight(), mark.getY());
    shield.lineTo (mark.getRight(), mark.getY() + mark.getHeight() * 0.55f);
    shield.quadraticTo (mark.getRight(), mark.getBottom() - 4.0f, mark.getCentreX(), mark.getBottom());
    shield.quadraticTo (mark.getX(), mark.getBottom() - 4.0f, mark.getX(), mark.getY() + mark.getHeight() * 0.55f);
    shield.closeSubPath();
    g.setColour (accent);
    g.strokePath (shield, juce::PathStrokeType (2.0f));
    g.drawLine (mark.getCentreX(), mark.getY() + 5.0f, mark.getCentreX(), mark.getBottom() - 6.0f, 2.0f);
    g.drawLine (mark.getX() + 5.0f, mark.getY() + 10.0f, mark.getRight() - 5.0f, mark.getY() + 10.0f, 2.0f);

    logo.removeFromLeft (8.0f);
    g.setColour (text);
    g.setFont (font (22.0f, true));
    g.drawText ("SKJALD", logo, juce::Justification::centredLeft);
}

void MainWindow::refresh (double dt, int frame)
{
    meters.refresh (dt);
    if (eqTab.isVisible())
        eqTab.refresh();
    if (maxTab.isVisible())
        maxTab.refresh (dt);
    if (frame % 8 == 0)
    {
        globalPresets.refresh();
        updateABButtons();
    }
}

} // namespace skjald::ui
