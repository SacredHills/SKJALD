#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Presets/PresetManager.h"

namespace skjald
{
class SkjaldAudioProcessor;
}

namespace skjald::ui
{
/** Compact preset bar for one category: previous / name (menu) / next / save.
    The menu lists factory and user presets and offers Save As, Delete and opening the
    user preset folder. */
class PresetBrowser : public juce::Component
{
public:
    PresetBrowser (SkjaldAudioProcessor& p, PresetCategory category);

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void showMenu();
    void step (int delta);
    void saveAs();

    SkjaldAudioProcessor& processor;
    PresetCategory category;
    juce::TextButton prev { "<" }, next { ">" }, nameButton, save { "Save" };
    juce::String shownName;
};

} // namespace skjald::ui
