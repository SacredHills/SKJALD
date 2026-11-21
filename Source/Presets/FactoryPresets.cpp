#include "PresetManager.h"
#include "../Plugin/Parameters.h"

namespace skjald
{
namespace
{
    using P = params::BandParam;
    using V = std::map<juce::String, float>;

    /** Sets band b (0-based) and enables it. */
    void band (V& v, int b, FilterType t, float freq, float gain, float q, int slopeIndex = 1)
    {
        v[params::bandID (b, P::On)] = 1.0f;
        v[params::bandID (b, P::Type)] = (float) (int) t;
        v[params::bandID (b, P::Freq)] = freq;
        v[params::bandID (b, P::Gain)] = gain;
        v[params::bandID (b, P::Q)] = q;
        v[params::bandID (b, P::Slope)] = (float) slopeIndex;
    }

    struct MaxSettings
    {
        LimiterMode mode = LimiterMode::Modern;
        float threshold = 0.0f, ceiling = -1.0f, release = 100.0f;
        bool autoRelease = false;
        float lookahead = 5.0f, link = 100.0f, transient = 0.0f;
        bool truePeak = true;
        int oversampling = 2;
        bool adaptive = true, softClip = false;
        int linkMode = 0;
        float inputGain = 0.0f;
        float character = 5.0f;
    };

    void maxValues (V& v, const MaxSettings& m)
    {
        v[params::mode] = (float) (int) m.mode;
        v[params::threshold] = m.threshold;
        v[params::ceiling] = m.ceiling;
        v[params::release] = m.release;
        v[params::autoRelease] = m.autoRelease ? 1.0f : 0.0f;
        v[params::lookahead] = m.lookahead;
        v[params::stereoLink] = m.link;
        v[params::transient] = m.transient;
        v[params::truePeak] = m.truePeak ? 1.0f : 0.0f;
        v[params::oversampling] = (float) m.oversampling;
        v[params::adaptiveRelease] = m.adaptive ? 1.0f : 0.0f;
        v[params::softClip] = m.softClip ? 1.0f : 0.0f;
        v[params::linkMode] = (float) m.linkMode;
        v[params::inputGain] = m.inputGain;
        v[params::character] = m.character;
    }

    // ---- EQ curves (bands keep their default slots: 0 HP, 1 LS, 2-5 bells, 6 HS, 7 LP) ----
    V eqClean()
    {
        V v;
        band (v, 0, FilterType::HighPass, 25.0f, 0.0f, 0.707f, 3);
        band (v, 6, FilterType::HighShelf, 12000.0f, 0.5f, 0.6f);
        return v;
    }
    V eqWarm()
    {
        V v;
        band (v, 0, FilterType::HighPass, 25.0f, 0.0f, 0.707f, 3);
        band (v, 1, FilterType::LowShelf, 120.0f, 1.5f, 0.7f);
        band (v, 4, FilterType::Bell, 3500.0f, -1.0f, 0.8f);
        band (v, 6, FilterType::HighShelf, 12000.0f, -0.8f, 0.6f);
        return v;
    }
    V eqBright()
    {
        V v;
        band (v, 0, FilterType::HighPass, 25.0f, 0.0f, 0.707f, 3);
        band (v, 2, FilterType::Bell, 300.0f, -0.8f, 1.0f);
        band (v, 6, FilterType::HighShelf, 8000.0f, 2.0f, 0.6f);
        return v;
    }
    V eqLowEnd()
    {
        V v;
        band (v, 0, FilterType::HighPass, 30.0f, 0.0f, 0.707f, 3);
        band (v, 1, FilterType::LowShelf, 80.0f, -1.0f, 0.7f);
        band (v, 2, FilterType::Bell, 160.0f, -1.5f, 1.4f);
        return v;
    }
    V eqMud()
    {
        V v;
        band (v, 0, FilterType::HighPass, 30.0f, 0.0f, 0.707f, 3);
        band (v, 2, FilterType::Bell, 250.0f, -2.5f, 1.4f);
        band (v, 3, FilterType::Bell, 500.0f, -1.0f, 1.0f);
        return v;
    }
    V eqAir()
    {
        V v;
        band (v, 0, FilterType::HighPass, 25.0f, 0.0f, 0.707f, 3);
        band (v, 4, FilterType::Bell, 4000.0f, 1.2f, 0.9f);
        band (v, 6, FilterType::HighShelf, 14000.0f, 2.5f, 0.7f);
        return v;
    }
    V eqGentle()
    {
        V v;
        band (v, 0, FilterType::HighPass, 22.0f, 0.0f, 0.707f, 3);
        band (v, 1, FilterType::LowShelf, 90.0f, 0.7f, 0.7f);
        band (v, 4, FilterType::Bell, 2500.0f, 0.5f, 0.7f);
        band (v, 6, FilterType::HighShelf, 12000.0f, 0.8f, 0.6f);
        return v;
    }
    V eqMetal()
    {
        V v;
        band (v, 0, FilterType::HighPass, 35.0f, 0.0f, 0.707f, 3);
        band (v, 2, FilterType::Bell, 90.0f, 1.0f, 1.0f);
        band (v, 3, FilterType::Bell, 400.0f, -2.0f, 1.2f);
        band (v, 5, FilterType::Bell, 6500.0f, -1.5f, 2.0f);
        band (v, 6, FilterType::HighShelf, 12000.0f, 1.0f, 0.7f);
        return v;
    }
    V eqRock()
    {
        V v;
        band (v, 0, FilterType::HighPass, 30.0f, 0.0f, 0.707f, 3);
        band (v, 1, FilterType::LowShelf, 100.0f, 1.0f, 0.7f);
        band (v, 3, FilterType::Bell, 500.0f, -1.0f, 1.0f);
        band (v, 4, FilterType::Bell, 2000.0f, 1.0f, 0.8f);
        band (v, 6, FilterType::HighShelf, 10000.0f, 1.0f, 0.7f);
        return v;
    }

    // ---- Maximizer settings ----
    MaxSettings mxTransparent() { MaxSettings m; m.mode = LimiterMode::Transparent; m.threshold = -3.0f; m.release = 150.0f; return m; }
    MaxSettings mxGentle() { MaxSettings m; m.mode = LimiterMode::Smooth; m.threshold = -2.0f; m.release = 250.0f; return m; }
    MaxSettings mxModernLoud() { MaxSettings m; m.mode = LimiterMode::Modern; m.threshold = -7.0f; m.release = 80.0f; m.autoRelease = true; return m; }
    MaxSettings mxLoudMetal() { MaxSettings m; m.mode = LimiterMode::Aggressive; m.threshold = -10.0f; m.ceiling = -0.8f; m.release = 40.0f; m.softClip = true; m.transient = 20.0f; m.oversampling = 3; return m; }
    MaxSettings mxModernMetal() { MaxSettings m; m.mode = LimiterMode::Modern; m.threshold = -8.0f; m.release = 60.0f; m.transient = 30.0f; m.softClip = true; return m; }
    MaxSettings mxPunchyRock() { MaxSettings m; m.mode = LimiterMode::Punch; m.threshold = -6.0f; m.release = 90.0f; m.transient = 40.0f; return m; }
    MaxSettings mxHeavyRock() { MaxSettings m; m.mode = LimiterMode::Aggressive; m.threshold = -7.0f; m.release = 70.0f; m.transient = 20.0f; return m; }
    MaxSettings mxElectronic() { MaxSettings m; m.mode = LimiterMode::Modern; m.threshold = -8.0f; m.ceiling = -0.8f; m.release = 50.0f; m.lookahead = 3.0f; m.oversampling = 3; m.link = 80.0f; m.linkMode = 1; return m; }
    MaxSettings mxAcoustic() { MaxSettings m; m.mode = LimiterMode::Smooth; m.threshold = -2.5f; m.release = 300.0f; m.transient = 10.0f; return m; }
    MaxSettings mxSmooth() { MaxSettings m; m.mode = LimiterMode::Smooth; m.threshold = -4.0f; m.release = 200.0f; m.autoRelease = true; return m; }
    MaxSettings mxHighLoudness() { MaxSettings m; m.mode = LimiterMode::Aggressive; m.threshold = -12.0f; m.ceiling = -0.5f; m.release = 30.0f; m.softClip = true; m.oversampling = 4; m.transient = 25.0f; return m; }

    Preset make (const char* name, PresetCategory c, V v)
    {
        Preset p;
        p.name = name;
        p.category = c;
        p.factory = true;
        p.values = std::move (v);
        return p;
    }

    Preset makeMax (const char* name, const MaxSettings& m)
    {
        V v;
        maxValues (v, m);
        return make (name, PresetCategory::Maximizer, std::move (v));
    }

    Preset makeGlobal (const char* name, V eq, const MaxSettings& m)
    {
        maxValues (eq, m);
        return make (name, PresetCategory::Global, std::move (eq));
    }
} // namespace

const juce::Array<Preset>& PresetManager::factoryPresets()
{
    static const juce::Array<Preset> presets = []
    {
        juce::Array<Preset> a;
        // EQ
        a.add (make ("Clean Master", PresetCategory::EQ, eqClean()));
        a.add (make ("Warm Master", PresetCategory::EQ, eqWarm()));
        a.add (make ("Bright Master", PresetCategory::EQ, eqBright()));
        a.add (make ("Low-End Control", PresetCategory::EQ, eqLowEnd()));
        a.add (make ("Mud Removal", PresetCategory::EQ, eqMud()));
        a.add (make ("Air and Presence", PresetCategory::EQ, eqAir()));
        a.add (make ("Gentle Mastering", PresetCategory::EQ, eqGentle()));
        a.add (make ("Modern Metal EQ", PresetCategory::EQ, eqMetal()));
        a.add (make ("Rock Master EQ", PresetCategory::EQ, eqRock()));
        // Maximizer
        a.add (makeMax ("Transparent Master", mxTransparent()));
        a.add (makeMax ("Gentle Limiting", mxGentle()));
        a.add (makeMax ("Modern Loudness", mxModernLoud()));
        a.add (makeMax ("Loud Metal", mxLoudMetal()));
        a.add (makeMax ("Modern Metal", mxModernMetal()));
        a.add (makeMax ("Punchy Rock", mxPunchyRock()));
        a.add (makeMax ("Heavy Rock", mxHeavyRock()));
        a.add (makeMax ("Electronic Master", mxElectronic()));
        a.add (makeMax ("Acoustic Master", mxAcoustic()));
        a.add (makeMax ("Smooth Master", mxSmooth()));
        a.add (makeMax ("High Loudness", mxHighLoudness()));
        // Global (EQ + Maximizer)
        a.add (makeGlobal ("Clean Master", eqClean(), mxTransparent()));
        a.add (makeGlobal ("Warm Master", eqWarm(), mxSmooth()));
        a.add (makeGlobal ("Rock Master", eqRock(), mxPunchyRock()));
        a.add (makeGlobal ("Metal Master", eqMetal(), mxModernMetal()));
        a.add (makeGlobal ("Loud Metal Master", eqMetal(), mxLoudMetal()));
        a.add (makeGlobal ("Electronic Master", eqAir(), mxElectronic()));
        a.add (makeGlobal ("Acoustic Master", eqGentle(), mxAcoustic()));
        a.add (makeGlobal ("Streaming Safe", eqClean(), mxGentle()));
        return a;
    }();
    return presets;
}

} // namespace skjald
