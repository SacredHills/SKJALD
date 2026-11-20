#include "PresetManager.h"
#include "../Plugin/Parameters.h"

namespace skjald
{
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

juce::String PresetManager::categoryName (PresetCategory c)
{
    switch (c)
    {
        case PresetCategory::Global: return "Global";
        case PresetCategory::EQ: return "EQ";
        case PresetCategory::Maximizer: return "Maximizer";
    }
    return {};
}

juce::StringArray PresetManager::parameterIDs (PresetCategory c)
{
    juce::StringArray ids;
    if (c != PresetCategory::Maximizer)
        ids.addArray (params::eqParameterIDs());
    if (c != PresetCategory::EQ)
        ids.addArray (params::maximizerParameterIDs());
    // bypass and monitoring switches are working states, not part of a sound preset
    ids.removeString (params::eqBypass);
    ids.removeString (params::maxBypass);
    ids.removeString (params::delta);
    return ids;
}

juce::File PresetManager::getUserDirectory (PresetCategory c)
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("SKJALD")
        .getChildFile ("Presets")
        .getChildFile (categoryName (c));
}

Preset PresetManager::readFile (const juce::File& f) const
{
    Preset p;
    p.factory = false;
    p.file = f;
    p.name = f.getFileNameWithoutExtension();
    if (auto xml = juce::XmlDocument::parse (f))
    {
        if (xml->hasTagName ("SKJALDPRESET"))
        {
            p.name = xml->getStringAttribute ("name", p.name);
            p.category = (PresetCategory) juce::jlimit (0, 2, xml->getIntAttribute ("category", 0));
            for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
                p.values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        }
    }
    return p;
}

juce::Array<Preset> PresetManager::getPresets (PresetCategory c) const
{
    juce::Array<Preset> out;
    for (const auto& p : factoryPresets())
        if (p.category == c)
            out.add (p);

    const auto dir = getUserDirectory (c);
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
        files.sort();
        for (const auto& f : files)
        {
            auto p = readFile (f);
            p.category = c;
            if (! p.values.empty())
                out.add (p);
        }
    }
    return out;
}

void PresetManager::load (const Preset& preset)
{
    for (const auto& id : parameterIDs (preset.category))
    {
        auto* param = state.getParameter (id);
        if (param == nullptr)
            continue;
        float normalised = param->getDefaultValue();
        if (auto it = preset.values.find (id); it != preset.values.end())
            normalised = param->convertTo0to1 (it->second);
        if (std::abs (param->getValue() - normalised) > 1.0e-7f)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (normalised);
            param->endChangeGesture();
        }
    }
    setCurrentName (preset.category, preset.name);
    if (preset.category == PresetCategory::Global)
    {
        setCurrentName (PresetCategory::EQ, {});
        setCurrentName (PresetCategory::Maximizer, {});
    }
}

bool PresetManager::saveUserPreset (const juce::String& name, PresetCategory c)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    const auto dir = getUserDirectory (c);
    if (! dir.createDirectory())
        return false;

    juce::XmlElement xml ("SKJALDPRESET");
    xml.setAttribute ("name", name.trim());
    xml.setAttribute ("category", (int) c);
    xml.setAttribute ("version", 1);
    for (const auto& id : parameterIDs (c))
        if (auto* param = state.getParameter (id))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", id);
            e->setAttribute ("value", param->convertFrom0to1 (param->getValue()));
        }
    const bool ok = xml.writeTo (dir.getChildFile (clean + fileExtension));
    if (ok)
        setCurrentName (c, name.trim());
    return ok;
}

bool PresetManager::deleteUserPreset (const Preset& p)
{
    return ! p.factory && p.file.existsAsFile() && p.file.deleteFile();
}

} // namespace skjald
