#include "ABComparison.h"
#include "Parameters.h"

namespace skjald
{
ABComparison::ABComparison (juce::AudioProcessor& p) : proc (p) {}

juce::ValueTree ABComparison::capture() const
{
    juce::ValueTree t ("SNAPSHOT");
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != params::globalBypass)
                t.setProperty (rp->getParameterID(), rp->getValue(), nullptr);
    return t;
}

void ABComparison::apply (const juce::ValueTree& snapshot)
{
    if (! snapshot.isValid())
        return;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == params::globalBypass || ! snapshot.hasProperty (id))
                continue;
            const float v = (float) snapshot.getProperty (id);
            if (std::abs (v - rp->getValue()) > 1.0e-7f)
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (v);
                rp->endChangeGesture();
            }
        }
}

void ABComparison::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == active)
        return;
    slots[active] = capture();
    if (! slots[slot].isValid())
        slots[slot] = slots[active].createCopy(); // first visit: start from the current settings
    apply (slots[slot]);
    active = slot;
}

void ABComparison::copyActiveToOther()
{
    slots[1 - active] = capture();
}

juce::ValueTree ABComparison::toValueTree() const
{
    juce::ValueTree t ("AB");
    t.setProperty ("active", active, nullptr);
    // the active slot is always represented by the live parameters
    for (int s = 0; s < 2; ++s)
    {
        juce::ValueTree slot ("SLOT");
        slot.setProperty ("index", s, nullptr);
        const auto snap = (s == active) ? capture() : slots[s];
        if (snap.isValid())
            slot.appendChild (snap.createCopy(), nullptr);
        t.appendChild (slot, nullptr);
    }
    return t;
}

void ABComparison::fromValueTree (const juce::ValueTree& t)
{
    if (! t.hasType ("AB"))
        return;
    active = juce::jlimit (0, 1, (int) t.getProperty ("active", 0));
    for (int i = 0; i < t.getNumChildren(); ++i)
    {
        const auto slot = t.getChild (i);
        const int idx = juce::jlimit (0, 1, (int) slot.getProperty ("index", 0));
        if (slot.getNumChildren() > 0)
            slots[idx] = slot.getChild (0).createCopy();
    }
}

} // namespace skjald
