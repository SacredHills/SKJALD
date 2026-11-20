#include "EQGraph.h"
#include "../Common/Theme.h"
#include "../../Plugin/PluginProcessor.h"
#include "DSP/Filters/FilterDesign.h"

namespace skjald::ui
{
namespace
{
    constexpr float kMinF = 20.0f, kMaxF = 20000.0f;
    constexpr float kSpectrumTop = 0.0f, kSpectrumBottom = -90.0f;
    constexpr float kNodeRadius = 8.0f;
}

EQGraph::EQGraph (SkjaldAudioProcessor& p) : processor (p)
{
    auto& s = p.getState();
    for (int b = 0; b < kNumEQBands; ++b)
    {
        using BP = params::BandParam;
        bands[(size_t) b] = { s.getParameter (params::bandID (b, BP::On)), s.getParameter (params::bandID (b, BP::Type)),
                              s.getParameter (params::bandID (b, BP::Freq)), s.getParameter (params::bandID (b, BP::Gain)),
                              s.getParameter (params::bandID (b, BP::Q)), s.getParameter (params::bandID (b, BP::Slope)) };
    }
    processor.getTaps().analyzerActive = true;
    setRepaintsOnMouseActivity (false);
}

EQGraph::~EQGraph()
{
    processor.getTaps().analyzerActive = false;
}

juce::Rectangle<float> EQGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedLeft (34.0f).withTrimmedBottom (18.0f).withTrimmedRight (34.0f);
}

float EQGraph::freqToX (float f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * std::log (juce::jlimit (kMinF, kMaxF, f) / kMinF) / std::log (kMaxF / kMinF);
}

float EQGraph::xToFreq (float x) const
{
    const auto a = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth());
    return kMinF * std::pow (kMaxF / kMinF, t);
}

float EQGraph::dbToY (float db) const
{
    const auto a = plotArea();
    return a.getCentreY() - db / rangeDb * (a.getHeight() * 0.5f - 8.0f);
}

float EQGraph::yToDb (float y) const
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f - 8.0f) * rangeDb;
}

float EQGraph::spectrumDbToY (float db) const
{
    const auto a = plotArea();
    const float t = (db - kSpectrumBottom) / (kSpectrumTop - kSpectrumBottom);
    return a.getBottom() - juce::jlimit (0.0f, 1.0f, t) * a.getHeight();
}

bool EQGraph::typeHasGain (FilterType t) { return filterTypeUsesGain (t); }

juce::Point<float> EQGraph::nodePosition (int b, const EQParams& eq) const
{
    const auto& bp = eq.bands[(size_t) b];
    const float y = typeHasGain (bp.type) ? dbToY (juce::jlimit (-rangeDb, rangeDb, bp.gainDb)) : dbToY (0.0f);
    return { freqToX (bp.frequency), y };
}

int EQGraph::hitTestNode (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = kNodeRadius * 1.8f;
    for (int b = kNumEQBands - 1; b >= 0; --b)
    {
        const float d = nodePosition (b, cachedEQ).getDistanceFrom (p);
        if (d < bestDist)
        {
            bestDist = d;
            best = b;
        }
    }
    return best;
}

void EQGraph::setSelectedBand (int b)
{
    selected = juce::jlimit (0, kNumEQBands - 1, b);
    repaint();
}

void EQGraph::resized()
{
    gridImage = {};
}

void EQGraph::refresh()
{
    const double fs = processor.getCurrentSampleRate();
    preAnalyzer.setSampleRate (fs);
    postAnalyzer.setSampleRate (fs);
    preAnalyzer.update (processor.getTaps().pre);
    postAnalyzer.update (processor.getTaps().post);
    cachedEQ = params::readEQ (processor.getState());
    repaint();
}

void EQGraph::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto area = plotArea();
    const double fs = juce::jmax (44100.0, processor.getCurrentSampleRate());

    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // ---- grid ----
    {
        g.saveState();
        const int freqs[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900,
                              1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
        for (int f : freqs)
        {
            const bool major = f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000
                               || f == 5000 || f == 10000 || f == 20000;
            g.setColour (major ? gridStrong : grid);
            g.drawVerticalLine ((int) std::round (freqToX ((float) f)), area.getY(), area.getBottom());
        }
        g.setFont (font (11.0f));
        g.setColour (textFaint);
        for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            const juce::String label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
            g.drawText (label, juce::Rectangle<float> (freqToX (f) - 20.0f, area.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centred);
        }
        const float step = rangeDb <= 6.0f ? 1.0f : (rangeDb <= 12.0f ? 3.0f : 6.0f);
        for (float db = -rangeDb; db <= rangeDb + 0.01f; db += step)
        {
            const float y = dbToY (db);
            g.setColour (std::abs (db) < 0.01f ? outlineStrong : grid);
            g.drawHorizontalLine ((int) std::round (y), area.getX(), area.getRight());
            g.setColour (textFaint);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (area.getRight() + 2.0f, y - 7.0f, 30.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
        // spectrum dB scale on the left
        for (float db = -80.0f; db <= -10.0f; db += 20.0f)
        {
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, spectrumDbToY (db) - 7.0f, 30.0f, 14.0f),
                        juce::Justification::centredRight);
        }
        g.restoreState();
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // ---- spectrum ----
    auto spectrumPath = [this, &area] (const SpectrumAnalyzer& a, bool closed)
    {
        juce::Path p;
        const auto& lv = a.getLevels();
        for (int i = 0; i < SpectrumAnalyzer::kPoints; ++i)
        {
            const float x = freqToX (SpectrumAnalyzer::pointFrequency (i));
            const float y = spectrumDbToY (lv[(size_t) i]);
            if (i == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
        if (closed)
        {
            p.lineTo (area.getRight(), area.getBottom());
            p.lineTo (area.getX(), area.getBottom());
            p.closeSubPath();
        }
        return p;
    };
    if (analyzerMode >= 1)
    {
        const auto post = spectrumPath (postAnalyzer, true);
        g.setGradientFill (juce::ColourGradient (analysis.withAlpha (0.22f), 0.0f, area.getY(), analysis.withAlpha (0.03f), 0.0f, area.getBottom(), false));
        g.fillPath (post);
        g.setColour (analysis.withAlpha (0.45f));
        g.strokePath (spectrumPath (postAnalyzer, false), juce::PathStrokeType (1.0f));
    }
    if (analyzerMode >= 2)
    {
        g.setColour (textDim.withAlpha (0.35f));
        g.strokePath (spectrumPath (preAnalyzer, false), juce::PathStrokeType (1.0f));
    }

    // ---- per-band responses and the combined curve ----
    const int numX = juce::jmax (64, (int) area.getWidth() / 2);
    std::vector<double> combined ((size_t) numX, 1.0);
    std::array<SectionCascade, kNumEQBands> cascades;
    for (int b = 0; b < kNumEQBands; ++b)
        cascades[(size_t) b] = FilterDesign::designBand (cachedEQ.bands[(size_t) b], fs);

    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto& bp = cachedEQ.bands[(size_t) b];
        if (! bp.enabled)
            continue;
        juce::Path fill;
        fill.startNewSubPath (area.getX(), dbToY (0.0f));
        for (int i = 0; i < numX; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) (numX - 1);
            const double w = kTwoPi * xToFreq (x) / fs;
            const double m = cascades[(size_t) b].magnitude (w);
            combined[(size_t) i] *= m;
            const float db = (float) juce::jlimit (-60.0, 60.0, 20.0 * std::log10 (juce::jmax (1.0e-6, m)));
            fill.lineTo (x, dbToY (db));
        }
        fill.lineTo (area.getRight(), dbToY (0.0f));
        fill.closeSubPath();
        const auto c = band (b);
        const bool sel = b == selected;
        g.setColour (c.withAlpha (sel ? 0.20f : 0.09f));
        g.fillPath (fill);
        if (sel)
        {
            g.setColour (c.withAlpha (0.55f));
            g.strokePath (fill, juce::PathStrokeType (1.0f));
        }
    }

    if (! cachedEQ.bypass)
    {
        juce::Path curve;
        for (int i = 0; i < numX; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) (numX - 1);
            const float db = (float) juce::jlimit (-60.0, 60.0, 20.0 * std::log10 (juce::jmax (1.0e-6, combined[(size_t) i])));
            if (i == 0) curve.startNewSubPath (x, dbToY (db));
            else curve.lineTo (x, dbToY (db));
        }
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.restoreState();

    // ---- nodes ----
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto& bp = cachedEQ.bands[(size_t) b];
        const auto pos = nodePosition (b, cachedEQ);
        const auto c = band (b);
        const bool sel = b == selected, hov = b == hovered;
        const float r = kNodeRadius + (sel ? 2.0f : 0.0f) + (hov ? 1.0f : 0.0f);
        const auto circle = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos);
        if (bp.enabled && ! cachedEQ.bypass)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (circle.expanded (3.0f));
            g.setColour (c);
            g.fillEllipse (circle);
            g.setColour (background);
        }
        else
        {
            g.setColour (panel);
            g.fillEllipse (circle);
            g.setColour (c.withAlpha (0.6f));
            g.drawEllipse (circle, 1.5f);
            g.setColour (c.withAlpha (0.8f));
        }
        g.setFont (font (11.0f, true));
        g.drawText (juce::String (b + 1), circle, juce::Justification::centred);
        if (sel)
        {
            g.setColour (text.withAlpha (0.9f));
            g.drawEllipse (circle.expanded (2.0f), 1.0f);
        }
    }

    // ---- readout for hovered / dragged node ----
    const int info = dragging >= 0 ? dragging : hovered;
    if (info >= 0)
    {
        const auto& bp = cachedEQ.bands[(size_t) info];
        juce::String s = juce::String (info + 1) + "  " + params::filterTypeNames()[(int) bp.type] + "   ";
        s += bp.frequency >= 1000.0f ? juce::String (bp.frequency / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (bp.frequency)) + " Hz";
        if (typeHasGain (bp.type))
            s += "   " + formatDb (bp.gainDb, 1) + " dB";
        if (filterTypeUsesSlope (bp.type))
            s += "   " + params::slopeNames()[bp.slopeIndex];
        if (! (filterTypeUsesSlope (bp.type) && bp.slopeIndex == 0)) // 6 dB/oct sections have no Q
            s += "   Q " + juce::String (bp.q, 2);
        if (! bp.enabled)
            s += "   (off)";
        const auto pos = nodePosition (info, cachedEQ);
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (font (12.5f), s) + 16.0f;
        auto box = juce::Rectangle<float> (w, 22.0f).withCentre ({ pos.x, pos.y - 26.0f });
        if (box.getY() < area.getY()) box.setY (pos.y + 16.0f);
        box = box.constrainedWithin (area);
        g.setColour (panelRaised.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (band (info).withAlpha (0.8f));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (text);
        g.setFont (font (12.5f));
        g.drawText (s, box, juce::Justification::centred);
    }

    if (cachedEQ.bypass)
    {
        g.setColour (textDim);
        g.setFont (font (13.0f, true));
        g.drawText ("EQ BYPASSED", area.reduced (10.0f), juce::Justification::topLeft);
    }
}

void EQGraph::setParam (juce::RangedAudioParameter* p, float realValue)
{
    if (p != nullptr)
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void EQGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTestNode (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EQGraph::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void EQGraph::mouseDown (const juce::MouseEvent& e)
{
    const int h = hitTestNode (e.position);
    if (h < 0)
        return;

    setSelectedBand (h);
    if (onBandSelected)
        onBandSelected (h);

    auto& bp = bands[(size_t) h];
    if (e.mods.isPopupMenu())
    {
        showBandMenu (h);
        return;
    }
    if (e.mods.isAltDown())
    {
        bp.on->beginChangeGesture();
        bp.on->setValueNotifyingHost (bp.on->getValue() >= 0.5f ? 0.0f : 1.0f);
        bp.on->endChangeGesture();
        return;
    }

    dragging = h;
    dragStart = e.position;
    dragStartFreq = bp.freq->convertFrom0to1 (bp.freq->getValue());
    dragStartGain = bp.gain->convertFrom0to1 (bp.gain->getValue());
    // grabbing a disabled node switches it on
    if (bp.on->getValue() < 0.5f)
    {
        bp.on->beginChangeGesture();
        bp.on->setValueNotifyingHost (1.0f);
        bp.on->endChangeGesture();
    }
    bp.freq->beginChangeGesture();
    bp.gain->beginChangeGesture();
}

void EQGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    auto& bp = bands[(size_t) dragging];
    const bool fine = e.mods.isShiftDown();
    const float scale = fine ? 0.15f : 1.0f;

    const float dx = (e.position.x - dragStart.x) * scale;
    const float newFreq = xToFreq (freqToX (dragStartFreq) + dx);
    setParam (bp.freq, juce::jlimit (20.0f, 20000.0f, newFreq));

    const auto type = (FilterType) juce::roundToInt (bp.type->convertFrom0to1 (bp.type->getValue()));
    if (typeHasGain (type))
    {
        const float dy = (e.position.y - dragStart.y) * scale;
        const float newGain = yToDb (dbToY (dragStartGain) + dy);
        setParam (bp.gain, juce::jlimit (-24.0f, 24.0f, newGain));
    }
    cachedEQ = params::readEQ (processor.getState());
    repaint();
}

void EQGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    auto& bp = bands[(size_t) dragging];
    bp.freq->endChangeGesture();
    bp.gain->endChangeGesture();
    dragging = -1;
    repaint();
}

void EQGraph::resetBand (int b)
{
    const auto& d = params::bandDefaults()[(size_t) b];
    auto& bp = bands[(size_t) b];
    auto set = [] (juce::RangedAudioParameter* p, float real)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (real));
        p->endChangeGesture();
    };
    set (bp.type, (float) (int) d.type);
    set (bp.freq, d.frequency);
    set (bp.gain, d.gainDb);
    set (bp.q, d.q);
    set (bp.slope, (float) d.slopeIndex);
}

void EQGraph::addBandAt (juce::Point<float> p)
{
    // Prefer a disabled band whose default slot is nearest in frequency
    const float f = xToFreq (p.x);
    int best = -1;
    float bestDist = 1.0e9f;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        if (cachedEQ.bands[(size_t) b].enabled)
            continue;
        const float d = std::abs (std::log (params::bandDefaults()[(size_t) b].frequency / f));
        if (d < bestDist) { bestDist = d; best = b; }
    }
    if (best < 0)
        return;

    auto& bp = bands[(size_t) best];
    auto set = [] (juce::RangedAudioParameter* prm, float real)
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
        prm->endChangeGesture();
    };
    set (bp.type, (float) (int) FilterType::Bell);
    set (bp.freq, f);
    set (bp.gain, juce::jlimit (-24.0f, 24.0f, yToDb (p.y)));
    set (bp.q, 1.0f);
    set (bp.on, 1.0f);
    setSelectedBand (best);
    if (onBandSelected)
        onBandSelected (best);
}

void EQGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitTestNode (e.position);
    if (h >= 0)
        resetBand (h);
    else if (plotArea().contains (e.position))
        addBandAt (e.position);
    cachedEQ = params::readEQ (processor.getState());
    repaint();
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitTestNode (e.position);
    if (b < 0)
        b = (hovered >= 0) ? hovered : -1;
    if (b < 0)
        return;
    auto& bp = bands[(size_t) b];
    const float q = bp.q->convertFrom0to1 (bp.q->getValue());
    const float step = e.mods.isShiftDown() ? 1.02f : 1.12f;
    const float delta = (std::abs (w.deltaY) > 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    const float newQ = juce::jlimit (0.1f, 10.0f, q * std::pow (step, delta * 4.0f));
    bp.q->beginChangeGesture();
    setParam (bp.q, newQ);
    bp.q->endChangeGesture();
    cachedEQ = params::readEQ (processor.getState());
    repaint();
}

void EQGraph::showBandMenu (int b)
{
    auto& bp = bands[(size_t) b];
    const int type = juce::roundToInt (bp.type->convertFrom0to1 (bp.type->getValue()));
    const int slope = juce::roundToInt (bp.slope->convertFrom0to1 (bp.slope->getValue()));
    const bool on = bp.on->getValue() >= 0.5f;

    juce::PopupMenu m;
    m.addSectionHeader ("Band " + juce::String (b + 1));
    m.addItem (1, on ? "Disable band" : "Enable band");
    m.addItem (2, "Reset band");
    m.addSeparator();
    juce::PopupMenu types;
    for (int t = 0; t < kNumFilterTypes; ++t)
        types.addItem (100 + t, params::filterTypeNames()[t], true, t == type);
    m.addSubMenu ("Filter type", types);
    juce::PopupMenu slopes;
    for (int s = 0; s < 6; ++s)
        slopes.addItem (200 + s, params::slopeNames()[s], filterTypeUsesSlope ((FilterType) type), s == slope);
    m.addSubMenu ("Slope", slopes, filterTypeUsesSlope ((FilterType) type));

    juce::Component::SafePointer<EQGraph> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, b] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& p = safe->bands[(size_t) b];
        auto set = [] (juce::RangedAudioParameter* prm, float normalised)
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (normalised);
            prm->endChangeGesture();
        };
        if (r == 1) set (p.on, p.on->getValue() >= 0.5f ? 0.0f : 1.0f);
        else if (r == 2) safe->resetBand (b);
        else if (r >= 100 && r < 100 + kNumFilterTypes) set (p.type, p.type->convertTo0to1 ((float) (r - 100)));
        else if (r >= 200 && r < 206) set (p.slope, p.slope->convertTo0to1 ((float) (r - 200)));
        safe->cachedEQ = params::readEQ (safe->processor.getState());
        safe->repaint();
    });
}

} // namespace skjald::ui
