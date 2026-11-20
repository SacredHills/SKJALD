#include "PresetBrowser.h"
#include "Common/Theme.h"
#include "../Plugin/PluginProcessor.h"

namespace skjald::ui
{
PresetBrowser::PresetBrowser (SkjaldAudioProcessor& p, PresetCategory c) : processor (p), category (c)
{
    for (auto* b : { &prev, &next, &nameButton, &save })
        addAndMakeVisible (b);
    prev.setTooltip ("Previous " + PresetManager::categoryName (c) + " preset");
    next.setTooltip ("Next " + PresetManager::categoryName (c) + " preset");
    save.setTooltip ("Save the current " + PresetManager::categoryName (c) + " settings as a user preset");
    nameButton.setTooltip (PresetManager::categoryName (c) + " presets");
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    nameButton.onClick = [this] { showMenu(); };
    save.onClick = [this] { saveAs(); };
    refresh();
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (52));
    r.removeFromRight (4);
    next.setBounds (r.removeFromRight (26));
    prev.setBounds (r.removeFromLeft (26));
    r.reduce (4, 0);
    nameButton.setBounds (r);
}

void PresetBrowser::paint (juce::Graphics&) {}

void PresetBrowser::refresh()
{
    auto n = processor.getPresets().getCurrentName (category);
    if (n.isEmpty())
        n = PresetManager::categoryName (category) + ": (custom)";
    if (n != shownName)
    {
        shownName = n;
        nameButton.setButtonText (n);
    }
}

void PresetBrowser::step (int delta)
{
    auto& pm = processor.getPresets();
    const auto list = pm.getPresets (category);
    if (list.isEmpty())
        return;
    int idx = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == pm.getCurrentName (category))
            idx = i;
    idx = (idx < 0) ? (delta > 0 ? 0 : list.size() - 1) : (idx + delta + list.size()) % list.size();
    pm.load (list[idx]);
    refresh();
}

void PresetBrowser::showMenu()
{
    auto& pm = processor.getPresets();
    const auto list = pm.getPresets (category);
    const auto current = pm.getCurrentName (category);

    juce::PopupMenu m;
    m.addSectionHeader ("Factory");
    int userStart = -1;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].factory && userStart < 0)
        {
            userStart = i;
            m.addSectionHeader ("User");
        }
        m.addItem (i + 1, list[i].name, true, list[i].name == current);
    }
    m.addSeparator();
    m.addItem (10001, "Save as...");
    bool currentIsUser = false;
    for (const auto& p : list)
        if (! p.factory && p.name == current)
            currentIsUser = true;
    m.addItem (10002, "Delete \"" + current + "\"", currentIsUser);
    m.addItem (10003, "Open user preset folder");

    juce::Component::SafePointer<PresetBrowser> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (nameButton.getWidth()),
                     [safe, list] (int r)
                     {
                         if (safe == nullptr || r == 0)
                             return;
                         auto& presets = safe->processor.getPresets();
                         if (r >= 1 && r <= list.size())
                             presets.load (list[r - 1]);
                         else if (r == 10001)
                             safe->saveAs();
                         else if (r == 10002)
                         {
                             for (const auto& p : list)
                                 if (! p.factory && p.name == presets.getCurrentName (safe->category))
                                     presets.deleteUserPreset (p);
                             presets.setCurrentName (safe->category, {});
                         }
                         else if (r == 10003)
                         {
                             auto dir = PresetManager::getUserDirectory (safe->category);
                             dir.createDirectory();
                             dir.startAsProcess();
                         }
                         safe->refresh();
                     });
}

void PresetBrowser::saveAs()
{
    auto* w = new juce::AlertWindow ("Save " + PresetManager::categoryName (category) + " preset",
                                     "Enter a name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", processor.getPresets().getCurrentName (category), "Name:");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBrowser> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            const auto name = w->getTextEditorContents ("name");
            if (! safe->processor.getPresets().saveUserPreset (name, safe->category))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "SKJALD", "The preset could not be saved.");
            safe->refresh();
        }
    }), true);
}

} // namespace skjald::ui
