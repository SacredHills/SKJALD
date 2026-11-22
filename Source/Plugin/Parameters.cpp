#include "Parameters.h"

namespace skjald::params
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using FloatParam = juce::AudioParameterFloat;
using BoolParam = juce::AudioParameterBool;
using ChoiceParam = juce::AudioParameterChoice;
using Attributes = juce::AudioParameterFloatAttributes;

namespace
{
    constexpr int kVersion = 1;

    juce::NormalisableRange<float> logRange (float lo, float hi, float interval = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    juce::String formatDb (float v, int) { return juce::String (v, 1) + " dB"; }
    juce::String formatMs (float v, int) { return v < 100.0f ? juce::String (v, 1) + " ms" : juce::String ((int) std::round (v)) + " ms"; }
    juce::String formatPercent (float v, int) { return juce::String ((int) std::round (v)) + " %"; }
    juce::String formatHz (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
    }
    juce::String formatQ (float v, int) { return juce::String (v, 2); }
} // namespace

juce::String bandID (int b, BandParam which)
{
    static const char* names[] = { "On", "Type", "Freq", "Gain", "Q", "Slope" };
    return "b" + juce::String (b + 1) + names[(int) which];
}

const std::array<BandDefaults, kNumEQBands>& bandDefaults()
{
    static const std::array<BandDefaults, kNumEQBands> d { {
        { FilterType::HighPass, 30.0f, 0.0f, 0.707f, 3 },
        { FilterType::LowShelf, 100.0f, 0.0f, 0.707f, 1 },
        { FilterType::Bell, 250.0f, 0.0f, 1.0f, 1 },
        { FilterType::Bell, 700.0f, 0.0f, 1.0f, 1 },
        { FilterType::Bell, 2000.0f, 0.0f, 1.0f, 1 },
        { FilterType::Bell, 5000.0f, 0.0f, 1.0f, 1 },
        { FilterType::HighShelf, 10000.0f, 0.0f, 0.707f, 1 },
        { FilterType::LowPass, 20000.0f, 0.0f, 0.707f, 3 },
    } };
    return d;
}

const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray s { "Bell", "Low Shelf", "High Shelf", "High Pass", "Low Pass", "Notch" };
    return s;
}

const juce::StringArray& slopeNames()
{
    static const juce::StringArray s { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    return s;
}

const juce::StringArray& modeNames()
{
    static const juce::StringArray s { "Transparent", "Modern", "Aggressive", "Punch", "Smooth" };
    return s;
}

const juce::StringArray& oversamplingNames()
{
    static const juce::StringArray s { "1x", "2x", "4x", "8x", "16x" };
    return s;
}

const juce::StringArray& linkModeNames()
{
    static const juce::StringArray s { "Standard", "Split" };
    return s;
}

Layout createLayout()
{
    Layout layout;
    auto id = [] (const juce::String& s) { return juce::ParameterID { s, kVersion }; };

    // Global
    layout.add (std::make_unique<BoolParam> (id (globalBypass), "Global Bypass", false));
    layout.add (std::make_unique<BoolParam> (id (eqBypass), "EQ Bypass", false));
    layout.add (std::make_unique<BoolParam> (id (maxBypass), "Maximizer Bypass", false));

    // EQ
    auto eqGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("eq", "Equalizer", "|");
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto& d = bandDefaults()[(size_t) b];
        const juce::String prefix = "Band " + juce::String (b + 1) + " ";
        eqGroup->addChild (std::make_unique<BoolParam> (id (bandID (b, BandParam::On)), prefix + "On", false));
        eqGroup->addChild (std::make_unique<ChoiceParam> (id (bandID (b, BandParam::Type)), prefix + "Type", filterTypeNames(), (int) d.type));
        eqGroup->addChild (std::make_unique<FloatParam> (id (bandID (b, BandParam::Freq)), prefix + "Frequency", logRange (20.0f, 20000.0f),
                                                         d.frequency, Attributes().withLabel ("Hz").withStringFromValueFunction (formatHz)));
        eqGroup->addChild (std::make_unique<FloatParam> (id (bandID (b, BandParam::Gain)), prefix + "Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
                                                         d.gainDb, Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));
        eqGroup->addChild (std::make_unique<FloatParam> (id (bandID (b, BandParam::Q)), prefix + "Q", logRange (0.1f, 10.0f),
                                                         d.q, Attributes().withStringFromValueFunction (formatQ)));
        eqGroup->addChild (std::make_unique<ChoiceParam> (id (bandID (b, BandParam::Slope)), prefix + "Slope", slopeNames(), d.slopeIndex));
    }
    layout.add (std::move (eqGroup));

    // Maximizer
    auto mx = std::make_unique<juce::AudioProcessorParameterGroup> ("max", "Maximizer", "|");
    mx->addChild (std::make_unique<FloatParam> (id (inputGain), "Input Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                                Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));
    mx->addChild (std::make_unique<FloatParam> (id (threshold), "Threshold", juce::NormalisableRange<float> (-30.0f, 0.0f, 0.01f), 0.0f,
                                                Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));
    mx->addChild (std::make_unique<FloatParam> (id (ceiling), "Output Ceiling", juce::NormalisableRange<float> (-12.0f, 0.0f, 0.01f), -1.0f,
                                                Attributes().withLabel ("dBTP").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " dB"; })));
    mx->addChild (std::make_unique<FloatParam> (id (release), "Release", logRange (5.0f, 1000.0f), 100.0f,
                                                Attributes().withLabel ("ms").withStringFromValueFunction (formatMs)));
    mx->addChild (std::make_unique<BoolParam> (id (autoRelease), "Auto Release", true));
    mx->addChild (std::make_unique<FloatParam> (id (character), "Character", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f,
                                                Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));
    mx->addChild (std::make_unique<FloatParam> (id (clipAmount), "Clip Amount", juce::NormalisableRange<float> (0.0f, 300.0f, 0.01f), 0.0f,
                                                Attributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String ((int) std::lround (v)) + " %"; })));
    mx->addChild (std::make_unique<BoolParam> (id (thresholdLink), "Threshold-Ceiling Link", false));
    mx->addChild (std::make_unique<FloatParam> (id (targetLufs), "Target Loudness", juce::NormalisableRange<float> (-30.0f, -5.0f, 0.01f), -14.0f,
                                                Attributes().withLabel ("LUFS").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " LUFS"; })));
    mx->addChild (std::make_unique<FloatParam> (id (lookahead), "Lookahead", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f,
                                                Attributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " ms"; })));
    mx->addChild (std::make_unique<FloatParam> (id (stereoLink), "Stereo Link", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                Attributes().withLabel ("%").withStringFromValueFunction (formatPercent)));
    mx->addChild (std::make_unique<FloatParam> (id (transient), "Transient Emphasis", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                                Attributes().withLabel ("%").withStringFromValueFunction (formatPercent)));
    mx->addChild (std::make_unique<ChoiceParam> (id (mode), "Limiting Mode", modeNames(), (int) LimiterMode::Modern));
    mx->addChild (std::make_unique<BoolParam> (id (truePeak), "True Peak", true));
    mx->addChild (std::make_unique<ChoiceParam> (id (oversampling), "Oversampling", oversamplingNames(), 2));
    mx->addChild (std::make_unique<BoolParam> (id (adaptiveRelease), "Adaptive Release", true));
    mx->addChild (std::make_unique<BoolParam> (id (softClip), "Soft Clip", false));
    mx->addChild (std::make_unique<ChoiceParam> (id (linkMode), "Stereo Link Mode", linkModeNames(), 0));
    mx->addChild (std::make_unique<BoolParam> (id (autoGain), "Auto Gain Compensation", false));
    mx->addChild (std::make_unique<BoolParam> (id (delta), "Delta", false));
    layout.add (std::move (mx));

    return layout;
}

juce::StringArray eqParameterIDs()
{
    juce::StringArray ids;
    for (int b = 0; b < kNumEQBands; ++b)
        for (int k = 0; k < 6; ++k)
            ids.add (bandID (b, (BandParam) k));
    ids.add (eqBypass);
    return ids;
}

juce::StringArray maximizerParameterIDs()
{
    return { inputGain, threshold, ceiling, release, autoRelease, character, clipAmount, thresholdLink, targetLufs, lookahead, stereoLink, transient, mode,
             truePeak, oversampling, adaptiveRelease, softClip, linkMode, autoGain, delta, maxBypass };
}

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const juce::String& pid)
    {
        auto* p = s.getRawParameterValue (pid);
        jassert (p != nullptr);
        return p;
    };
    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto& bp = bands[(size_t) b];
        bp.on = get (bandID (b, BandParam::On));
        bp.type = get (bandID (b, BandParam::Type));
        bp.freq = get (bandID (b, BandParam::Freq));
        bp.gain = get (bandID (b, BandParam::Gain));
        bp.q = get (bandID (b, BandParam::Q));
        bp.slope = get (bandID (b, BandParam::Slope));
    }
    pGlobalBypass = get (globalBypass);
    pEqBypass = get (eqBypass);
    pMaxBypass = get (maxBypass);
    pInputGain = get (inputGain);
    pThreshold = get (threshold);
    pCeiling = get (ceiling);
    pRelease = get (release);
    pAutoRelease = get (autoRelease);
    pCharacter = get (character);
    pClipAmount = get (clipAmount);
    pLookahead = get (lookahead);
    pStereoLink = get (stereoLink);
    pTransient = get (transient);
    pMode = get (mode);
    pTruePeak = get (truePeak);
    pOversampling = get (oversampling);
    pAdaptive = get (adaptiveRelease);
    pSoftClip = get (softClip);
    pLinkMode = get (linkMode);
    pAutoGain = get (autoGain);
    pDelta = get (delta);
}

EngineParams ParameterReader::read() const noexcept
{
    auto f = [] (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    auto b = [&f] (std::atomic<float>* p) { return f (p) >= 0.5f; };
    auto i = [&f] (std::atomic<float>* p) { return (int) std::lround (f (p)); };

    EngineParams e;
    for (int k = 0; k < kNumEQBands; ++k)
    {
        const auto& bp = bands[(size_t) k];
        auto& band = e.eq.bands[(size_t) k];
        band.enabled = b (bp.on);
        band.type = (FilterType) std::clamp (i (bp.type), 0, kNumFilterTypes - 1);
        band.frequency = f (bp.freq);
        band.gainDb = f (bp.gain);
        band.q = f (bp.q);
        band.slopeIndex = std::clamp (i (bp.slope), 0, 5);
    }
    e.eq.bypass = b (pEqBypass);
    e.globalBypass = b (pGlobalBypass);

    auto& m = e.max;
    m.inputGainDb = f (pInputGain);
    m.thresholdDb = f (pThreshold);
    m.ceilingDb = f (pCeiling);
    m.releaseMs = f (pRelease);
    m.autoRelease = b (pAutoRelease);
    m.character = f (pCharacter);
    m.clipAmount = f (pClipAmount) * 0.01f;
    m.lookaheadMs = f (pLookahead);
    m.stereoLink = f (pStereoLink) * 0.01f;
    m.transientEmphasis = f (pTransient) * 0.01f;
    m.mode = (LimiterMode) std::clamp (i (pMode), 0, kNumLimiterModes - 1);
    m.truePeak = b (pTruePeak);
    m.oversamplingIndex = std::clamp (i (pOversampling), 0, 4);
    m.adaptiveRelease = b (pAdaptive);
    m.softClip = b (pSoftClip);
    m.linkMode = (StereoLinkMode) std::clamp (i (pLinkMode), 0, 1);
    m.autoGain = b (pAutoGain);
    m.delta = b (pDelta);
    m.bypass = b (pMaxBypass);
    return e;
}

EQParams readEQ (juce::AudioProcessorValueTreeState& s)
{
    return ParameterReader (s).read().eq;
}

} // namespace skjald::params
