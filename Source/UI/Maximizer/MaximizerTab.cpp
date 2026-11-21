#include "../../Engine/LearnThreshold.h"
#include "MaximizerTab.h"
#include "../../Plugin/PluginProcessor.h"

namespace skjald::ui
{
// ---- GRHistoryDisplay -------------------------------------------------------------------
GRHistoryDisplay::GRHistoryDisplay (SkjaldAudioProcessor& p) : processor (p) {}

void GRHistoryDisplay::refresh (double dt)
{
    const double fs = juce::jmax (8000.0, processor.getCurrentSampleRate());
    const double samplesPerColumn = fs * kHistorySeconds / kColumns;

    for (;;)
    {
        const int n = processor.getTaps().gr.pull (scratch.data(), (int) scratch.size());
        if (n <= 0)
            break;
        for (int i = 0; i < n; ++i)
        {
            const auto& pt = scratch[(size_t) i];
            colPeak = juce::jmax (colPeak, pt.peakDb);
            avgSum += (double) pt.averageDb * pt.numSamples;
            accumulated += pt.numSamples;
            currentPeak = pt.peakDb;
            while (accumulated >= samplesPerColumn)
            {
                columns[(size_t) writePos] = { colPeak, (float) (avgSum / juce::jmax (1.0, accumulated)) };
                writePos = (writePos + 1) % kColumns;
                accumulated -= samplesPerColumn;
                avgSum = 0.0;
                colPeak = 0.0f;
            }
        }
    }

    // read-out ballistics and auto range (with hysteresis)
    displayPeak = currentPeak > displayPeak ? currentPeak : juce::jmax (currentPeak, displayPeak - (float) (dt * 12.0));
    float recentMax = 0.0f;
    for (const auto& c : columns)
        recentMax = juce::jmax (recentMax, c.peak);
    const float wanted = recentMax <= 2.5f ? 3.0f : (recentMax <= 5.0f ? 6.0f : (recentMax <= 11.0f ? 12.0f : 24.0f));
    if (wanted > rangeDb || wanted < rangeDb * 0.5f)
        rangeDb = wanted;
    repaint();
}

void GRHistoryDisplay::paint (juce::Graphics& g)
{
    using namespace colours;
    fillPanel (g, getLocalBounds().toFloat());
    auto bounds = getLocalBounds().reduced (12, 10);
    auto captionRow = bounds.removeFromTop (30);
    drawCaption (g, captionRow.removeFromTop (16), "Gain reduction");
    auto area = bounds.toFloat().withTrimmedRight (36.0f).withTrimmedTop (8.0f).withTrimmedBottom (18.0f);

    // grid
    const float step = rangeDb <= 3.0f ? 0.5f : (rangeDb <= 6.0f ? 1.0f : (rangeDb <= 12.0f ? 2.0f : 4.0f));
    g.setFont (font (11.0f));
    for (float db = 0.0f; db <= rangeDb + 0.001f; db += step)
    {
        const float y = area.getY() + area.getHeight() * db / rangeDb;
        g.setColour (db < 0.001f ? outlineStrong : grid);
        g.drawHorizontalLine ((int) std::round (y), area.getX(), area.getRight());
        g.setColour (textFaint);
        g.drawText ("-" + juce::String (db, step < 1.0f ? 1 : 0), juce::Rectangle<float> (area.getRight() + 4.0f, y - 7.0f, 32.0f, 14.0f), juce::Justification::centredLeft);
    }
    for (int s = 1; s < (int) kHistorySeconds; ++s)
    {
        const float x = area.getRight() - area.getWidth() * (float) s / (float) kHistorySeconds;
        g.setColour (grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    // history (newest on the right)
    juce::Path peakPath, avgPath;
    peakPath.startNewSubPath (area.getX(), area.getY());
    for (int i = 0; i < kColumns; ++i)
    {
        const auto& c = columns[(size_t) ((writePos + i) % kColumns)];
        const float x = area.getX() + area.getWidth() * (float) i / (float) (kColumns - 1);
        const float yp = area.getY() + area.getHeight() * juce::jlimit (0.0f, 1.0f, c.peak / rangeDb);
        const float ya = area.getY() + area.getHeight() * juce::jlimit (0.0f, 1.0f, c.avg / rangeDb);
        peakPath.lineTo (x, yp);
        if (i == 0) avgPath.startNewSubPath (x, ya);
        else avgPath.lineTo (x, ya);
    }
    peakPath.lineTo (area.getRight(), area.getY());
    peakPath.closeSubPath();

    g.setGradientFill (juce::ColourGradient (gainReduction.withAlpha (0.55f), 0.0f, area.getY(), gainReduction.withAlpha (0.15f), 0.0f, area.getBottom(), false));
    g.fillPath (peakPath);
    g.setColour (gainReduction);
    g.strokePath (peakPath, juce::PathStrokeType (1.2f));
    g.setColour (text.withAlpha (0.75f));
    g.strokePath (avgPath, juce::PathStrokeType (1.5f));

    // read-out (in the caption row, right aligned with the plot)
    auto ro = getLocalBounds().reduced (12, 6).removeFromTop (34).withRight ((int) area.getRight());
    g.setColour (displayPeak > 0.05f ? gainReduction : textDim);
    g.setFont (font (26.0f));
    g.drawText ((displayPeak > 0.05f ? "-" : "") + juce::String (displayPeak, 1) + " dB", ro, juce::Justification::centredRight);
    g.setFont (font (10.5f, true));
    g.setColour (textFaint);
    g.drawText ("PEAK (FILL)  /  AVERAGE (LINE)", getLocalBounds().reduced (16, 12).removeFromBottom (16), juce::Justification::centredLeft);
}

// ---- LoudnessPanel ----------------------------------------------------------------------
LoudnessPanel::LoudnessPanel (SkjaldAudioProcessor& p) : processor (p)
{
    grMeter.setReductionStyle (true);
    addAndMakeVisible (grMeter);
}

void LoudnessPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (20);
    grMeter.setBounds (r.removeFromRight (12).withTrimmedBottom (18));
}

void LoudnessPanel::refresh (double dt)
{
    grMeter.setLevel (processor.getEngine().getGainReductionDb(), dt);
    repaint();
}

void LoudnessPanel::mouseDown (const juce::MouseEvent&)
{
    processor.getEngine().getOutputLoudness().resetIntegrated();
    processor.getEngine().getOutputTruePeakMeter().requestMaxReset();
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    using namespace colours;
    fillPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 10);
    drawCaption (g, r.removeFromTop (16), "Loudness");
    r.removeFromRight (20);
    r.removeFromTop (6);

    auto& e = processor.getEngine();
    auto big = [&g] (juce::Rectangle<int>& area, const juce::String& label, float lufs, juce::Colour c, float size)
    {
        auto line = area.removeFromTop ((int) size + 22);
        g.setColour (textFaint);
        g.setFont (font (10.5f, true));
        g.drawText (label, line.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (font (size));
        g.drawText (lufs <= -99.0f ? juce::String ("-inf") : juce::String (lufs, 1), line, juce::Justification::centredLeft);
    };
    big (r, "SHORT-TERM  LUFS", e.getOutputLoudness().getShortTerm(), text, 34.0f);
    big (r, "INTEGRATED  LUFS", e.getOutputLoudness().getIntegrated(), accent, 28.0f);
    big (r, "MOMENTARY  LUFS", e.getOutputLoudness().getMomentary(), textDim, 20.0f);

    const float tp = juce::Decibels::gainToDecibels (e.getOutputTruePeakMeter().getMaxPeak(), -100.0f);
    auto line = r.removeFromTop (40);
    g.setColour (textFaint);
    g.setFont (font (10.5f, true));
    g.drawText ("TRUE PEAK MAX", line.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (text);
    g.setFont (font (18.0f));
    g.drawText (formatDb (tp, 2) + " dBTP", line, juce::Justification::centredLeft);

    const float agc = e.getAutoGainDb();
    if (processor.getState().getRawParameterValue (params::autoGain)->load() > 0.5f)
    {
        line = r.removeFromTop (36);
        g.setColour (textFaint);
        g.setFont (font (10.5f, true));
        g.drawText ("AUTO GAIN", line.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (analysis);
        g.setFont (font (16.0f));
        g.drawText (formatDb (agc, 1) + " dB", line, juce::Justification::centredLeft);
    }

    g.setColour (textFaint);
    g.setFont (font (10.0f, true));
    g.drawText ("GR", grMeter.getBounds().withY (grMeter.getBottom() + 2).withHeight (14).expanded (6, 0), juce::Justification::centred);
}

// ---- MaximizerTab -----------------------------------------------------------------------
MaximizerTab::MaximizerTab (SkjaldAudioProcessor& p)
    : processor (p), grDisplay (p), loudness (p),
      modeSelector (p.getState(), params::mode, params::modeNames(), true),
      inputGain (p.getState(), params::inputGain, "Input Gain", true),
      threshold (p.getState(), params::threshold, "Threshold"),
      ceiling (p.getState(), params::ceiling, "Ceiling"),
      character (p.getState(), params::character, "Character"),
      release (p.getState(), params::release, "Release"),
      stereoLink (p.getState(), params::stereoLink, "Stereo Link", false, colours::analysis),
      transient (p.getState(), params::transient, "Transient", false, colours::analysis),
      lookahead (p.getState(), params::lookahead, "Lookahead", false, colours::analysis),
      thresholdLink (p.getState(), params::thresholdLink, "LINK", colours::analysis),
      autoRelease (p.getState(), params::autoRelease, "AUTO"),
      truePeak (p.getState(), params::truePeak, "TRUE PEAK"),
      bypass (p.getState(), params::maxBypass, "MAX BYPASS"),
      delta (p.getState(), params::delta, "DELTA", colours::analysis),
      adaptive (p.getState(), params::adaptiveRelease, "ADAPTIVE RELEASE"),
      softClip (p.getState(), params::softClip, "SOFT CLIP"),
      autoGain (p.getState(), params::autoGain, "AUTO GAIN", colours::analysis),
      oversampling (p.getState(), params::oversampling, params::oversamplingNames()),
      linkMode (p.getState(), params::linkMode, params::linkModeNames()),
      presetBrowser (p, PresetCategory::Maximizer)
{
    modeSelector.setDescriptions ({ "Clean, natural, minimal artefacts", "Balanced commercial loudness",
                                    "Dense and loud, controlled drive", "Keeps kick and snare impact",
                                    "Gentle, for acoustic material" });
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &grDisplay, &loudness, &modeSelector, &inputGain, &threshold, &ceiling, &character, &release, &stereoLink, &transient,
             &autoRelease, &truePeak, &bypass, &delta, &oversampling, &advancedButton, &presetBrowser,
             &thresholdLink, &learnButton, &targetSlider })
        addAndMakeVisible (c);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lookahead, &adaptive, &softClip, &autoGain, &linkMode })
        addChildComponent (c);

    thresholdLink.setTooltip ("Link: moving the threshold moves the ceiling by the same amount");
    learnButton.setTooltip ("Learn Threshold: press, play the loudest part, press again. Sets the threshold for the target loudness");
    targetSlider.setSliderStyle (juce::Slider::LinearBar);
    targetSlider.setTextValueSuffix (" LUFS");
    targetSlider.setTooltip ("Target loudness (LUFS) used by Learn");
    targetAttachment = std::make_unique<APVTS::SliderAttachment> (p.getState(), params::targetLufs, targetSlider);

    lastThreshold = (float) threshold.getSlider().getValue();
    threshold.getSlider().onValueChange = [this]
    {
        const float now = (float) threshold.getSlider().getValue();
        const float delta_ = now - lastThreshold;
        lastThreshold = now;
        // only for user edits: preset loads and automation must not drag the ceiling along
        if (thresholdLink.getToggleState() && threshold.getSlider().isMouseOverOrDragging (true) && std::abs (delta_) > 1.0e-4f)
        {
            auto& cs = ceiling.getSlider();
            cs.setValue (std::clamp (cs.getValue() + delta_, cs.getMinimum(), cs.getMaximum()), juce::sendNotificationSync);
        }
    };

    learnButton.setClickingTogglesState (false);
    learnButton.onClick = [this]
    {
        auto& e = processor.getEngine();
        if (! learning)
        {
            e.getInputLoudness().resetIntegrated();
            e.getInputPeakMeter().reset();
            learning = true;
            learnButton.setButtonText ("APPLY");
            return;
        }
        learning = false;
        learnButton.setButtonText ("LEARN");
        const float t = learnedThresholdDb (e.getInputLoudness().getIntegrated(), (float) targetSlider.getValue(),
                                            (float) inputGain.getSlider().getValue(),
                                            juce::Decibels::gainToDecibels (e.getInputPeakMeter().getMaxPeak(), -100.0f),
                                            (float) ceiling.getSlider().getValue());
        threshold.getSlider().setValue (t, juce::sendNotificationSync);
    };
    delta.setTooltip ("Delta: listen to what the maximizer removes (latency aligned)");
    autoGain.setTooltip ("Automatic gain compensation: level-matched monitoring for A/B (never exceeds the ceiling)");
    oversampling.setTooltip ("Oversampling factor of the limiter (higher = less aliasing, more CPU)");
    linkMode.setTooltip ("Standard: one link amount. Split: sustained stage fully linked, peak stage follows Stereo Link");
    advancedButton.setClickingTogglesState (true);
    advancedButton.onClick = [this]
    {
        advancedOpen = advancedButton.getToggleState();
        for (juce::Component* c : std::initializer_list<juce::Component*> { &lookahead, &adaptive, &softClip, &autoGain, &linkMode })
            c->setVisible (advancedOpen);
        resized();
        repaint();
    };
}

void MaximizerTab::resized()
{
    auto r = getLocalBounds();
    auto toolbar = r.removeFromTop (34);
    bypass.setBounds (toolbar.removeFromLeft (110).reduced (0, 3));
    toolbar.removeFromLeft (8);
    delta.setBounds (toolbar.removeFromLeft (80).reduced (0, 3));
    toolbar.removeFromLeft (8);
    advancedButton.setBounds (toolbar.removeFromLeft (110).reduced (0, 3));
    presetBrowser.setBounds (toolbar.removeFromRight (300).reduced (0, 2));
    r.removeFromTop (8);

    // bottom: knobs
    knobArea = r.removeFromBottom (168);
    r.removeFromBottom (10);
    {
        auto k = knobArea.reduced (12, 10);
        k.removeFromTop (18);
        auto learnRow = k.removeFromBottom (22);
        thresholdLink.setBounds (learnRow.removeFromLeft (56).reduced (0, 1));
        learnRow.removeFromLeft (6);
        learnButton.setBounds (learnRow.removeFromLeft (64).reduced (0, 1));
        learnRow.removeFromLeft (6);
        targetSlider.setBounds (learnRow.removeFromLeft (120).reduced (0, 1));
        auto right = k.removeFromRight (230);
        const int kw = k.getWidth() / 7;
        inputGain.setBounds (k.removeFromLeft (kw));
        threshold.setBounds (k.removeFromLeft (kw));
        ceiling.setBounds (k.removeFromLeft (kw));
        character.setBounds (k.removeFromLeft (kw));
        auto rel = k.removeFromLeft (kw);
        autoRelease.setBounds (rel.removeFromBottom (24).withSizeKeepingCentre (64, 24));
        release.setBounds (rel);
        stereoLink.setBounds (k.removeFromLeft (kw));
        transient.setBounds (k.removeFromLeft (kw));

        right.removeFromLeft (14);
        right.removeFromTop (10);
        truePeak.setBounds (right.removeFromTop (30));
        right.removeFromTop (26);
        oversampling.setBounds (right.removeFromTop (30));
    }

    // left: algorithms
    modeArea = r.removeFromLeft (200);
    {
        auto m = modeArea.reduced (12, 10);
        m.removeFromTop (22);
        modeSelector.setBounds (m);
    }
    r.removeFromLeft (10);

    // right: loudness
    loudness.setBounds (r.removeFromRight (200));
    r.removeFromRight (10);

    // advanced strip (inside the centre column, below the GR display)
    if (advancedOpen)
    {
        advancedArea = r.removeFromBottom (112);
        r.removeFromBottom (10);
        auto a = advancedArea.reduced (12, 10);
        a.removeFromTop (18);
        lookahead.setCompact (true);
        lookahead.setBounds (a.removeFromLeft (90));
        a.removeFromLeft (12);
        auto col1 = a.removeFromLeft (170);
        adaptive.setBounds (col1.removeFromTop (30));
        col1.removeFromTop (14);
        softClip.setBounds (col1.removeFromTop (30));
        a.removeFromLeft (12);
        auto col2 = a.removeFromLeft (170);
        autoGain.setBounds (col2.removeFromTop (30));
        col2.removeFromTop (14);
        linkMode.setBounds (col2.removeFromTop (30));
    }
    else
    {
        advancedArea = {};
    }
    grDisplay.setBounds (r);
}

void MaximizerTab::paint (juce::Graphics& g)
{
    fillPanel (g, modeArea.toFloat());
    drawCaption (g, modeArea.reduced (12, 10).removeFromTop (16), "Algorithm");

    fillPanel (g, knobArea.toFloat());
    auto k = knobArea.reduced (12, 10);
    drawCaption (g, k.removeFromTop (16), "Limiter");
    auto right = k.removeFromRight (230).withTrimmedLeft (14);
    drawCaption (g, right.withY (right.getY() + 46).withHeight (16), "Oversampling");

    if (advancedOpen)
    {
        fillPanel (g, advancedArea.toFloat());
        drawCaption (g, advancedArea.reduced (12, 10).removeFromTop (16), "Advanced");
        auto a = advancedArea.reduced (12, 10);
        drawCaption (g, juce::Rectangle<int> (a.getX() + 90 + 12 + 170 + 12, a.getY() + 18 + 30 + 1, 170, 13), "Stereo link mode");
    }
}

void MaximizerTab::refresh (double dt)
{
    grDisplay.refresh (dt);
    loudness.refresh (dt);
    presetBrowser.refresh();
}

} // namespace skjald::ui
