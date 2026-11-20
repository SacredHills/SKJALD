#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace skjald
{
/** Two complete parameter snapshots (A and B). Switching stores the current settings in the
    active slot and applies the other one through the normal parameter path, so every change
    is smoothed by the DSP (no glitches) and the host sees regular parameter changes.
    The global bypass is not part of a snapshot. Message thread only. */
class ABComparison
{
public:
    explicit ABComparison (juce::AudioProcessor& processor);

    int getActiveSlot() const noexcept { return active; }
    void selectSlot (int slot);
    void copyActiveToOther();

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    juce::ValueTree capture() const;
    void apply (const juce::ValueTree& snapshot);

    juce::AudioProcessor& proc;
    juce::ValueTree slots[2];
    int active = 0;
};

} // namespace skjald
