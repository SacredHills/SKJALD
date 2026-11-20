#pragma once

#include "Common/Controls.h"
#include "Equalizer/EQTab.h"
#include "Maximizer/MaximizerTab.h"
#include "MeterComponents/Meters.h"
#include "PresetBrowser.h"

namespace skjald::ui
{
/** Two-tab navigation (EQUALIZER | MAXIMIZER). Switching tabs only changes which view is
    visible: processing, DSP state and parameters are never touched. */
class TabNavigation : public juce::Component
{
public:
    TabNavigation();
    void resized() override;
    void setCurrent (int index);
    int getCurrent() const noexcept { return current; }
    std::function<void (int)> onChange;

private:
    juce::TextButton eqTab { "EQUALIZER" }, maxTab { "MAXIMIZER" };
    int current = 0;
};

/** Root view at the fixed design size (1180 x 720). The editor scales it uniformly. */
class MainWindow : public juce::Component
{
public:
    static constexpr int kDesignWidth = 1180;
    static constexpr int kDesignHeight = 720;

    explicit MainWindow (SkjaldAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (double dt, int frame);

private:
    void showTab (int index);
    void updateABButtons();

    SkjaldAudioProcessor& processor;
    TabNavigation tabs;
    PresetBrowser globalPresets;
    juce::TextButton buttonA { "A" }, buttonB { "B" }, copyAB { "A>B" };
    ParamToggle globalBypass;
    MeterStrip meters;
    EQTab eqTab;
    MaximizerTab maxTab;
    juce::Rectangle<int> headerArea;
};

} // namespace skjald::ui
