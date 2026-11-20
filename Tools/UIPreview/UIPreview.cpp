// Development tool: runs the SKJALD processor and editor in one process without a DAW,
// feeds a synthetic mix through it and renders the interface to PNG files.
//
// Usage: skjald_ui_preview <output dir> [scale]

#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"

#include <cmath>

namespace
{
juce::Button* findButton (juce::Component* root, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::Button*> (root); b != nullptr && b->getButtonText() == text)
        return b;
    for (auto* c : root->getChildren())
        if (auto* f = findButton (c, text))
            return f;
    return nullptr;
}

void feed (skjald::SkjaldAudioProcessor& p, int blocks, int& counter)
{
    const double fs = 48000.0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (counter + 1);
    for (int b = 0; b < blocks; ++b, ++counter)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (counter * 512 + i) / fs;
            const double tb = std::fmod (t, 0.5);
            const double kick = std::sin (juce::MathConstants<double>::twoPi * (50.0 + 120.0 * std::exp (-tb * 30.0)) * tb) * std::exp (-tb * 9.0);
            const double hat = (rng.nextFloat() - 0.5) * std::exp (-std::fmod (t, 0.25) * 40.0) * 0.4;
            const double pad = 0.12 * (std::sin (juce::MathConstants<double>::twoPi * 220.0 * t) + std::sin (juce::MathConstants<double>::twoPi * 330.0 * t));
            const double bass = 0.3 * std::sin (juce::MathConstants<double>::twoPi * 55.0 * t);
            const double v = 0.8 * kick + hat + pad + bass + (rng.nextFloat() - 0.5) * 0.02;
            buf.setSample (0, i, (float) v);
            buf.setSample (1, i, (float) (v * 0.97));
        }
        p.processBlock (buf, midi);
    }
}

void setParam (skjald::SkjaldAudioProcessor& p, const juce::String& id, float real)
{
    if (auto* prm = p.getState().getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

void save (juce::Component& c, const juce::File& f, float scale)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui-preview");
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    out.createDirectory();

    skjald::SkjaldAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // a representative mastering setting
    using BP = skjald::params::BandParam;
    namespace P = skjald::params;
    setParam (proc, P::bandID (0, BP::On), 1.0f);
    setParam (proc, P::bandID (1, BP::On), 1.0f);
    setParam (proc, P::bandID (1, BP::Gain), 2.5f);
    setParam (proc, P::bandID (2, BP::On), 1.0f);
    setParam (proc, P::bandID (2, BP::Gain), -3.0f);
    setParam (proc, P::bandID (2, BP::Q), 1.4f);
    setParam (proc, P::bandID (4, BP::On), 1.0f);
    setParam (proc, P::bandID (4, BP::Gain), 1.5f);
    setParam (proc, P::bandID (6, BP::On), 1.0f);
    setParam (proc, P::bandID (6, BP::Gain), 3.0f);
    setParam (proc, P::threshold, -8.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1180, 720);

    int counter = 0;
    for (int round = 0; round < 60; ++round)
    {
        feed (proc, 4, counter);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    save (*ed, out.getChildFile ("eq_tab.png"), scale);

    if (auto* b = findButton (ed.get(), "MAXIMIZER"))
        b->triggerClick();
    for (int round = 0; round < 200; ++round)
    {
        feed (proc, 4, counter);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    save (*ed, out.getChildFile ("maximizer_tab.png"), scale);

    if (auto* b = findButton (ed.get(), "ADVANCED"))
        b->triggerClick();
    for (int round = 0; round < 20; ++round)
    {
        feed (proc, 4, counter);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    save (*ed, out.getChildFile ("maximizer_advanced.png"), scale);

    ed->setSize (826, 504);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, out.getChildFile ("maximizer_small.png"), 1.0f);

    ed.reset();
    proc.releaseResources();
    return 0;
}
