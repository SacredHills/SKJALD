#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Analyzer/SpectrumAnalyzer.h"
#include "../../Plugin/Parameters.h"

namespace skjald
{
class SkjaldAudioProcessor;
}

namespace skjald::ui
{
/** Interactive frequency-response display:
      - real-time pre/post-EQ spectrum
      - per-band response (fill) and the combined response (computed with the same filter
        design code that processes the audio, so it always shows the real curve)
      - eight draggable nodes: horizontal = frequency, vertical = gain (bell/shelf),
        mouse wheel = Q, shift = fine, double-click node = reset band,
        double-click empty area = add a band there, alt-click = enable/disable,
        right-click = band menu (type, slope, enable, reset). */
class EQGraph : public juce::Component
{
public:
    explicit EQGraph (SkjaldAudioProcessor& p);
    ~EQGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Called by the editor's timer. */
    void refresh();

    int getSelectedBand() const noexcept { return selected; }
    void setSelectedBand (int b);
    std::function<void (int)> onBandSelected;

    void setDisplayRange (float db) { rangeDb = db; repaint(); }
    float getDisplayRange() const noexcept { return rangeDb; }
    void setAnalyzerMode (int m) { analyzerMode = m; } // 0 off, 1 post, 2 pre + post

    void resetBand (int b);

private:
    struct BandParams
    {
        juce::RangedAudioParameter* on = nullptr;
        juce::RangedAudioParameter* type = nullptr;
        juce::RangedAudioParameter* freq = nullptr;
        juce::RangedAudioParameter* gain = nullptr;
        juce::RangedAudioParameter* q = nullptr;
        juce::RangedAudioParameter* slope = nullptr;
    };

    juce::Rectangle<float> plotArea() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float spectrumDbToY (float db) const;
    juce::Point<float> nodePosition (int band, const EQParams& eq) const;
    int hitTestNode (juce::Point<float> p) const;
    static bool typeHasGain (FilterType t);

    void setParam (juce::RangedAudioParameter* p, float realValue);
    void showBandMenu (int band);
    void addBandAt (juce::Point<float> p);

    SkjaldAudioProcessor& processor;
    std::array<BandParams, kNumEQBands> bands;
    SpectrumAnalyzer preAnalyzer, postAnalyzer;
    EQParams cachedEQ;

    int selected = 0, hovered = -1, dragging = -1;
    juce::Point<float> dragStart;
    float dragStartFreq = 1000.0f, dragStartGain = 0.0f;
    float rangeDb = 12.0f;
    int analyzerMode = 2;
    juce::Image gridImage;
};

} // namespace skjald::ui
