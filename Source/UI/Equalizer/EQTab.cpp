#include "EQTab.h"
#include "../../Plugin/PluginProcessor.h"

namespace skjald::ui
{
// ---- EQBandPanel ------------------------------------------------------------------------
EQBandPanel::EQBandPanel (SkjaldAudioProcessor& p) : processor (p)
{
    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto* btn = bandButtons.add (new juce::TextButton (juce::String (b + 1)));
        btn->getProperties().set ("accent", (juce::int64) colours::band (b).getARGB());
        btn->getProperties().set ("bold", true);
        btn->setTooltip ("Select band " + juce::String (b + 1));
        btn->onClick = [this, b]
        {
            setBand (b);
            if (onBandSelected)
                onBandSelected (b);
        };
        addAndMakeVisible (btn);
    }
    resetBand.onClick = [this]
    {
        const auto& d = params::bandDefaults()[(size_t) band];
        auto& s = processor.getState();
        using BP = params::BandParam;
        auto set = [&s] (const juce::String& id, float real)
        {
            auto* prm = s.getParameter (id);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (real));
            prm->endChangeGesture();
        };
        set (params::bandID (band, BP::Type), (float) (int) d.type);
        set (params::bandID (band, BP::Freq), d.frequency);
        set (params::bandID (band, BP::Gain), d.gainDb);
        set (params::bandID (band, BP::Q), d.q);
        set (params::bandID (band, BP::Slope), (float) d.slopeIndex);
    };
    addAndMakeVisible (resetBand);
    rebuildControls();
}

void EQBandPanel::setBand (int b)
{
    b = juce::jlimit (0, kNumEQBands - 1, b);
    if (b == band && freq != nullptr)
        return;
    band = b;
    rebuildControls();
    refresh();
}

void EQBandPanel::rebuildControls()
{
    auto& s = processor.getState();
    using BP = params::BandParam;
    const auto c = colours::band (band);
    enable = std::make_unique<ParamToggle> (s, params::bandID (band, BP::On), "ON", c);
    type = std::make_unique<ParamCombo> (s, params::bandID (band, BP::Type));
    slope = std::make_unique<ParamCombo> (s, params::bandID (band, BP::Slope));
    freq = std::make_unique<Knob> (s, params::bandID (band, BP::Freq), "Frequency", false, c);
    gain = std::make_unique<Knob> (s, params::bandID (band, BP::Gain), "Gain", true, c);
    q = std::make_unique<Knob> (s, params::bandID (band, BP::Q), "Q", false, c);
    for (juce::Component* comp : std::initializer_list<juce::Component*> { enable.get(), type.get(), slope.get(), freq.get(), gain.get(), q.get() })
        addAndMakeVisible (comp);
    type->onChange = [this] { refresh(); };
    resized();
}

void EQBandPanel::refresh()
{
    const auto eq = params::readEQ (processor.getState());
    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto* btn = bandButtons[b];
        btn->setToggleState (b == band, juce::dontSendNotification);
        btn->setAlpha (eq.bands[(size_t) b].enabled ? 1.0f : 0.55f);
    }
    const auto t = eq.bands[(size_t) band].type;
    if (gain != nullptr)
    {
        gain->setEnabled (filterTypeUsesGain (t));
        gain->getSlider().setEnabled (filterTypeUsesGain (t));
        slope->setEnabled (filterTypeUsesSlope (t));
        const bool usesQ = ! (filterTypeUsesSlope (t) && eq.bands[(size_t) band].slopeIndex == 0);
        q->setEnabled (usesQ);
        q->getSlider().setEnabled (usesQ);
    }
    repaint();
}

void EQBandPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto selector = r.removeFromLeft (300);
    selector.removeFromTop (20);
    {
        auto row = selector.removeFromTop (34);
        const int w = (row.getWidth() - 7 * 4) / 8;
        for (auto* b : bandButtons)
        {
            b->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (4);
        }
    }
    selector.removeFromTop (10);
    auto row2 = selector.removeFromTop (28);
    if (enable != nullptr)
        enable->setBounds (row2.removeFromLeft (56));
    row2.removeFromLeft (8);
    resetBand.setBounds (row2.removeFromLeft (110));

    r.removeFromLeft (20);
    auto combos = r.removeFromLeft (170);
    combos.removeFromTop (20);
    if (type != nullptr)
    {
        type->setBounds (combos.removeFromTop (30));
        combos.removeFromTop (12);
        slope->setBounds (combos.removeFromTop (30));
    }

    r.removeFromLeft (16);
    const int kw = juce::jmin (110, r.getWidth() / 3);
    if (freq != nullptr)
    {
        freq->setBounds (r.removeFromLeft (kw));
        gain->setBounds (r.removeFromLeft (kw));
        q->setBounds (r.removeFromLeft (kw));
    }
}

void EQBandPanel::paint (juce::Graphics& g)
{
    fillPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 8);
    drawCaption (g, r.removeFromTop (16).withWidth (300), "Bands");
    drawCaption (g, juce::Rectangle<int> (r.getX() + 320, r.getY() - 16, 170, 16), "Band " + juce::String (band + 1) + " filter");
}

// ---- EQTab ------------------------------------------------------------------------------
EQTab::EQTab (SkjaldAudioProcessor& p)
    : processor (p), graph (p), bandPanel (p), bypass (p.getState(), params::eqBypass, "EQ BYPASS"),
      presetBrowser (p, PresetCategory::EQ)
{
    addAndMakeVisible (graph);
    addAndMakeVisible (bandPanel);
    addAndMakeVisible (bypass);
    addAndMakeVisible (resetAll);
    addAndMakeVisible (analyzerButton);
    addAndMakeVisible (rangeButton);
    addAndMakeVisible (presetBrowser);

    graph.onBandSelected = [this] (int b) { bandPanel.setBand (b); };
    bandPanel.onBandSelected = [this] (int b) { graph.setSelectedBand (b); };

    resetAll.setTooltip ("Reset all EQ bands to their defaults (all bands off)");
    resetAll.onClick = [this]
    {
        auto& s = processor.getState();
        for (const auto& id : params::eqParameterIDs())
            if (auto* prm = s.getParameter (id); prm != nullptr && id != params::eqBypass)
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
            }
        processor.getPresets().setCurrentName (PresetCategory::EQ, {});
    };

    analyzerButton.onClick = [this]
    {
        analyzerMode = (analyzerMode + 2) % 3; // pre+post -> post -> off -> pre+post
        static const char* names[] = { "Analyzer: Off", "Analyzer: Post", "Analyzer: Pre + Post" };
        analyzerButton.setButtonText (names[analyzerMode]);
        graph.setAnalyzerMode (analyzerMode);
    };
    rangeButton.onClick = [this]
    {
        const float r = graph.getDisplayRange();
        const float next = r <= 6.0f ? 12.0f : (r <= 12.0f ? 24.0f : 6.0f);
        graph.setDisplayRange (next);
        rangeButton.setButtonText ("Range: " + juce::String ((int) next) + " dB");
    };
}

void EQTab::resized()
{
    auto r = getLocalBounds();
    auto toolbar = r.removeFromTop (34);
    bypass.setBounds (toolbar.removeFromLeft (100).reduced (0, 3));
    toolbar.removeFromLeft (8);
    resetAll.setBounds (toolbar.removeFromLeft (90).reduced (0, 3));
    toolbar.removeFromLeft (8);
    analyzerButton.setBounds (toolbar.removeFromLeft (150).reduced (0, 3));
    toolbar.removeFromLeft (8);
    rangeButton.setBounds (toolbar.removeFromLeft (110).reduced (0, 3));
    presetBrowser.setBounds (toolbar.removeFromRight (300).reduced (0, 2));

    r.removeFromTop (8);
    bandPanel.setBounds (r.removeFromBottom (150));
    r.removeFromBottom (10);
    graph.setBounds (r);
}

void EQTab::paint (juce::Graphics&) {}

void EQTab::refresh()
{
    graph.refresh();
    bandPanel.refresh();
    presetBrowser.refresh();
}

} // namespace skjald::ui
