#pragma once

#include "EQGraph.h"
#include "../Common/Controls.h"
#include "../PresetBrowser.h"

namespace skjald::ui
{
/** Band selector strip + detailed controls of the selected band. */
class EQBandPanel : public juce::Component
{
public:
    EQBandPanel (SkjaldAudioProcessor& p);
    void setBand (int b);
    int getBand() const noexcept { return band; }
    std::function<void (int)> onBandSelected;

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh(); // reflect band on/type state in the selector buttons

private:
    void rebuildControls();

    SkjaldAudioProcessor& processor;
    int band = 0;
    juce::OwnedArray<juce::TextButton> bandButtons;
    std::unique_ptr<ParamToggle> enable;
    std::unique_ptr<ParamCombo> type, slope;
    std::unique_ptr<Knob> freq, gain, q;
    juce::TextButton resetBand { "Reset band" };
};

class EQTab : public juce::Component
{
public:
    explicit EQTab (SkjaldAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    SkjaldAudioProcessor& processor;
    EQGraph graph;
    EQBandPanel bandPanel;
    ParamToggle bypass;
    juce::TextButton resetAll { "Reset EQ" };
    juce::TextButton analyzerButton { "Analyzer: Pre + Post" }, rangeButton { "Range: 12 dB" };
    PresetBrowser presetBrowser;
    int analyzerMode = 2;
};

} // namespace skjald::ui
