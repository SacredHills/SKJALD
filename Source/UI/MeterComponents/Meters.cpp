#include "Meters.h"
#include "../Common/Theme.h"
#include "../Common/Controls.h"
#include "../../Plugin/PluginProcessor.h"

namespace skjald::ui
{
float LevelMeter::dbToPos (float db, float minDb)
{
    return juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);
}

void LevelMeter::setLevel (float value, double dt)
{
    if (reduction)
    {
        // value is a positive gain reduction in dB: instant rise, 30 dB/s fall
        displayDb = value > displayDb ? value : juce::jmax (value, displayDb - (float) (dt * 30.0));
        repaint();
        return;
    }
    const float db = juce::Decibels::gainToDecibels (value, -100.0f);
    displayDb = db > displayDb ? db : juce::jmax (db, displayDb - (float) (dt * 20.0));
    holdTime += dt;
    if (db >= holdDb)
    {
        holdDb = db;
        holdTime = 0.0;
    }
    else if (holdTime > 1.5)
    {
        holdDb = displayDb;
        holdTime = 0.0;
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    using namespace colours;
    auto r = getLocalBounds().toFloat();
    g.setColour (background);
    g.fillRoundedRectangle (r, 2.0f);

    if (reduction)
    {
        const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, displayDb / 24.0f);
        g.setColour (gainReduction);
        g.fillRoundedRectangle (r.withHeight (h), 2.0f);
        return;
    }

    const float pos = dbToPos (displayDb, minDb);
    auto bar = r.withTrimmedTop (r.getHeight() * (1.0f - pos));
    juce::ColourGradient grad (ok.withAlpha (0.85f), 0.0f, r.getBottom(), danger, 0.0f, r.getY(), false);
    grad.addColour (dbToPos (-12.0f, minDb), ok.withAlpha (0.9f));
    grad.addColour (dbToPos (-3.0f, minDb), accent);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bar, 2.0f);

    if (holdDb > minDb)
    {
        const float y = r.getBottom() - r.getHeight() * dbToPos (holdDb, minDb);
        g.setColour (holdDb > -0.1f ? danger : text.withAlpha (0.8f));
        g.fillRect (r.getX(), y - 1.0f, r.getWidth(), 2.0f);
    }
}

// ---- MeterStrip -------------------------------------------------------------------------
MeterStrip::MeterStrip (SkjaldAudioProcessor& p) : processor (p)
{
    for (auto* m : { &inL, &inR, &outL, &outR })
        addAndMakeVisible (m);
    setTooltip ("Click a value to reset its hold (true peak / integrated loudness / peaks)");
}

void MeterStrip::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (20);
    readoutArea = r.removeFromBottom (232);
    r.removeFromBottom (18);
    auto meters = r;
    const int mw = 12, gap = 3, groupGap = 22;
    const int total = mw * 4 + gap * 2 + groupGap;
    meters = meters.withSizeKeepingCentre (total, meters.getHeight()).translated (10, 0);
    inL.setBounds (meters.removeFromLeft (mw));
    meters.removeFromLeft (gap);
    inR.setBounds (meters.removeFromLeft (mw));
    meters.removeFromLeft (groupGap);
    outL.setBounds (meters.removeFromLeft (mw));
    meters.removeFromLeft (gap);
    outR.setBounds (meters.removeFromLeft (mw));
}

void MeterStrip::refresh (double dt)
{
    auto& e = processor.getEngine();
    const float il = e.getInputPeakMeter().getBlockPeak (0), ir = e.getInputPeakMeter().getBlockPeak (1);
    const float ol = e.getOutputPeakMeter().getBlockPeak (0), orr = e.getOutputPeakMeter().getBlockPeak (1);
    inL.setLevel (il, dt);
    inR.setLevel (ir, dt);
    outL.setLevel (ol, dt);
    outR.setLevel (orr, dt);
    inPeakHold = e.getInputPeakMeter().getMaxPeak();
    outPeakHold = e.getOutputPeakMeter().getMaxPeak();
    repaint (readoutArea);
}

void MeterStrip::mouseDown (const juce::MouseEvent& ev)
{
    if (! readoutArea.contains (ev.getPosition()))
        return;
    auto& e = processor.getEngine();
    e.getOutputTruePeakMeter().requestMaxReset();
    e.getOutputLoudness().resetIntegrated();
    e.getOutputRms().resetIntegrated();
    e.getInputPeakMeter().reset();
    e.getOutputPeakMeter().reset();
}

void MeterStrip::paint (juce::Graphics& g)
{
    using namespace colours;
    fillPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 10);
    drawCaption (g, r.removeFromTop (16), "Meters", juce::Justification::centred);

    // meter labels and scale
    g.setFont (font (11.0f, true));
    g.setColour (textFaint);
    const auto inArea = inL.getBounds().getUnion (inR.getBounds());
    const auto outArea = outL.getBounds().getUnion (outR.getBounds());
    g.drawText ("IN", inArea.withY (inArea.getBottom() + 2).withHeight (14), juce::Justification::centred);
    g.drawText ("OUT", outArea.withY (outArea.getBottom() + 2).withHeight (14).expanded (6, 0), juce::Justification::centred);
    g.setFont (font (10.0f));
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f })
    {
        const float y = (float) inArea.getBottom() - (float) inArea.getHeight() * LevelMeter::dbToPos (db, -48.0f);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> ((float) inArea.getX() - 30.0f, y - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }

    // numeric readouts
    auto& e = processor.getEngine();
    auto row = [&g] (juce::Rectangle<int>& area, const juce::String& label, const juce::String& value, juce::Colour c)
    {
        auto line = area.removeFromTop (38);
        g.setColour (textFaint);
        g.setFont (font (10.5f, true));
        g.drawText (label, line.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (font (18.0f));
        g.drawText (value, line, juce::Justification::centredLeft);
    };
    auto ra = readoutArea;
    const float tp = juce::Decibels::gainToDecibels (e.getOutputTruePeakMeter().getMaxPeak(), -100.0f);
    const float ceiling = processor.getState().getRawParameterValue (params::ceiling)->load();
    row (ra, "INPUT PEAK", formatDb (juce::Decibels::gainToDecibels (inPeakHold, -100.0f)) + " dB", text);
    row (ra, "OUTPUT PEAK", formatDb (juce::Decibels::gainToDecibels (outPeakHold, -100.0f)) + " dB", text);
    row (ra, "TRUE PEAK", formatDb (tp, 2) + " dBTP", tp > ceiling + 0.05f ? danger : text);
    // follows the LUFS / RMS switch of the Maximizer tab's loudness panel
    const bool rms = (bool) processor.getState().state.getProperty ("meterShowsRms", false);
    const float li = rms ? e.getOutputRms().getIntegrated() : e.getOutputLoudness().getIntegrated();
    const float ls = rms ? e.getOutputRms().getShortTerm() : e.getOutputLoudness().getShortTerm();
    const juce::String unit = rms ? " RMS" : " LUFS";
    row (ra, "INTEGRATED", (li <= -99.0f ? juce::String ("-inf") : juce::String (li, 1)) + unit, accent);
    row (ra, "SHORT-TERM", (ls <= -99.0f ? juce::String ("-inf") : juce::String (ls, 1)) + unit, text);
    g.setColour (textFaint);
    g.setFont (font (10.0f));
    g.drawText ("click values to reset", ra, juce::Justification::centredLeft);
}

} // namespace skjald::ui
