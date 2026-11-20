// Plug-in level tests: loads the built VST3 exactly like a host does and checks
// latency reporting, sample rates, mono/stereo, variable block sizes, 64-bit processing,
// state save/restore, automation, ceiling enforcement, CPU and (optionally) renders the
// editor to PNG files.
//
// Usage: skjald_plugin_tests <path/to/SKJALD.vst3> [--snapshots <dir>]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>

namespace
{
int failures = 0, checks = 0;

void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("    FAIL: %s\n", what.toRawUTF8());
    }
}

void info (const juce::String& s) { std::printf ("      %s\n", s.toRawUTF8()); }

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& fm, const juce::String& path, double fs, int block)
{
    juce::OwnedArray<juce::PluginDescription> types;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, path);
    if (types.isEmpty())
        return {};
    juce::String error;
    auto p = fm.createPluginInstance (*types[0], fs, block, error);
    if (p == nullptr)
        std::printf ("load error: %s\n", error.toRawUTF8());
    return p;
}

juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name)
            return prm;
    return nullptr;
}

void setReal (juce::AudioProcessorParameter* prm, float normalised)
{
    if (prm != nullptr)
        prm->setValueNotifyingHost (normalised);
}

float peakOf (const juce::AudioBuffer<float>& b, int from = 0)
{
    float m = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        m = juce::jmax (m, b.getMagnitude (c, from, b.getNumSamples() - from));
    return m;
}

void fillMix (juce::AudioBuffer<float>& b, double fs, int offset)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (i + offset) / fs;
        const double kick = std::sin (juce::MathConstants<double>::twoPi * 55.0 * t) * std::exp (-std::fmod (t, 0.5) * 8.0);
        const double v = 0.9 * kick + 0.3 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t);
        for (int c = 0; c < b.getNumChannels(); ++c)
            b.setSample (c, i, (float) (v * (c == 0 ? 1.0 : 0.95)));
    }
}

juce::Component* findButton (juce::Component* root, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::Button*> (root); b != nullptr && b->getButtonText() == text)
        return b;
    for (auto* c : root->getChildren())
        if (auto* f = findButton (c, text))
            return f;
    return nullptr;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: skjald_plugin_tests <SKJALD.vst3> [--snapshots dir]\n");
        return 2;
    }
    const juce::String path = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).getFullPathName();
    juce::File snapshotDir;
    for (int i = 2; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshots")
            snapshotDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);

    juce::AudioPluginFormatManager fm;
    fm.addFormat (new juce::VST3PluginFormat());

    // ---- 1. latency, sample rates, ceiling, NaN-free processing ----------------------------
    std::printf ("[ RUN  ] sample rates, latency, ceiling\n");
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        auto p = load (fm, path, fs, 512);
        check (p != nullptr, "plugin loads at " + juce::String (fs));
        if (p == nullptr)
            return 1;
        p->setPlayConfigDetails (2, 2, fs, 512);
        p->prepareToPlay (fs, 512);
        const int latency = p->getLatencySamples();
        info (juce::String (fs, 0) + " Hz: latency " + juce::String (latency) + " samples (" + juce::String (1000.0 * latency / fs, 2) + " ms)");
        check (latency > 0 && latency < (int) (fs * 0.03), "plausible latency at " + juce::String (fs));

        setReal (findParam (*p, "Threshold"), 0.6f);   // -12 dB
        setReal (findParam (*p, "Output Ceiling"), 1.0f - 1.0f / 12.0f); // -1 dB
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        float maxPeak = 0.0f;
        bool finite = true;
        for (int blk = 0; blk < (int) (fs * 2 / 512); ++blk)
        {
            fillMix (buf, fs, blk * 512);
            p->processBlock (buf, midi);
            if (blk > 10)
                maxPeak = juce::jmax (maxPeak, peakOf (buf));
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    finite = finite && std::isfinite (buf.getSample (c, i));
        }
        check (finite, "finite output at " + juce::String (fs));
        check (juce::Decibels::gainToDecibels (maxPeak) <= -1.0f + 1.0e-3f, "ceiling kept at " + juce::String (fs) + " (peak " + juce::String (juce::Decibels::gainToDecibels (maxPeak), 3) + ")");

        // latency must not change with parameters
        setReal (findParam (*p, "Lookahead"), 0.1f);
        setReal (findParam (*p, "Oversampling"), 1.0f);
        p->processBlock (buf, midi);
        check (p->getLatencySamples() == latency, "latency constant when parameters change");
        p->releaseResources();
    }

    // ---- 2. latency-compensated alignment through the hosted plug-in -------------------------
    std::printf ("[ RUN  ] latency alignment\n");
    {
        const double fs = 48000.0;
        auto p = load (fm, path, fs, 256);
        p->setPlayConfigDetails (2, 2, fs, 256);
        p->prepareToPlay (fs, 256);
        const int latency = p->getLatencySamples();
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        int found = -1;
        for (int blk = 0; blk < 20 && found < 0; ++blk)
        {
            buf.clear();
            if (blk == 1)
                for (int c = 0; c < 2; ++c)
                    buf.setSample (c, 10, 0.3f);
            p->processBlock (buf, midi);
            for (int i = 0; i < 256; ++i)
                if (std::abs (buf.getSample (0, i)) > 0.15f)
                {
                    found = blk * 256 + i - (256 + 10);
                    break;
                }
        }
        check (found == latency, "impulse delayed by exactly the reported latency (" + juce::String (found) + " vs " + juce::String (latency) + ")");
    }

    // ---- 3. mono, variable block sizes, 64-bit ------------------------------------------------
    std::printf ("[ RUN  ] mono, variable block sizes, double precision\n");
    {
        const double fs = 44100.0;
        auto p = load (fm, path, fs, 1024);
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (p->setBusesLayout (mono), "mono layout accepted");
        p->prepareToPlay (fs, 1024);
        juce::AudioBuffer<float> buf (1, 1024);
        juce::MidiBuffer midi;
        bool finite = true;
        juce::Random rng (3);
        for (int blk = 0; blk < 200; ++blk)
        {
            const int n = 1 + rng.nextInt (1024);
            juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 1, n);
            for (int i = 0; i < n; ++i)
                sub.setSample (0, i, rng.nextFloat() * 2.0f - 1.0f);
            p->processBlock (sub, midi);
            for (int i = 0; i < n; ++i)
                finite = finite && std::isfinite (sub.getSample (0, i));
        }
        check (finite, "mono + random block sizes produce finite output");
        p->releaseResources();

        auto pd = load (fm, path, fs, 512);
        if (pd->supportsDoublePrecisionProcessing())
        {
            pd->setProcessingPrecision (juce::AudioProcessor::doublePrecision);
            pd->setPlayConfigDetails (2, 2, fs, 512);
            pd->prepareToPlay (fs, 512);
            juce::AudioBuffer<double> dbuf (2, 512);
            float m = 0.0f;
            for (int blk = 0; blk < 100; ++blk)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        dbuf.setSample (c, i, std::sin ((blk * 512 + i) * 0.05) * 1.5);
                pd->processBlock (dbuf, midi);
                if (blk > 5)
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 512; ++i)
                            m = juce::jmax (m, (float) std::abs (dbuf.getSample (c, i)));
            }
            check (m > 0.5f && m <= 1.0f, "64-bit processing works and is limited (peak " + juce::String (m, 3) + ")");
        }
        else
            check (false, "double precision supported");
    }

    // ---- 4. state save / restore and automation ---------------------------------------------
    std::printf ("[ RUN  ] state and automation\n");
    {
        const double fs = 48000.0;
        auto p = load (fm, path, fs, 512);
        p->setPlayConfigDetails (2, 2, fs, 512);
        p->prepareToPlay (fs, 512);
        juce::Random rng (11);
        std::vector<float> values;
        for (auto* prm : p->getParameters())
        {
            const float v = rng.nextFloat();
            prm->setValueNotifyingHost (v);
            values.push_back (prm->getValue());
        }
        // VST3 parameter changes reach the plug-in with the next audio block
        {
            juce::AudioBuffer<float> b (2, 64);
            juce::MidiBuffer m;
            b.clear();
            p->processBlock (b, m);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }
        juce::MemoryBlock state;
        p->getStateInformation (state);
        for (auto* prm : p->getParameters())
            prm->setValueNotifyingHost (prm->getDefaultValue());

        auto p2 = load (fm, path, fs, 512);
        p2->setPlayConfigDetails (2, 2, fs, 512);
        p2->prepareToPlay (fs, 512);
        p2->setStateInformation (state.getData(), (int) state.getSize());
        {
            juce::AudioBuffer<float> b (2, 64);
            juce::MidiBuffer m;
            b.clear();
            p2->processBlock (b, m);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }
        int mismatches = 0;
        for (int i = 0; i < p2->getParameters().size(); ++i)
        {
            auto* prm = p2->getParameters()[i];
            const int steps = prm->getNumSteps();
            // discrete / quantised parameters snap to their step grid
            // continuous parameters are stored with their 0.01-unit interval: allow that quantisation
            const float tol = (steps > 1 && steps < 100000) ? 0.5f / (float) (steps - 1) + 1.0e-5f : 5.0e-4f;
            if (std::abs (prm->getValue() - values[(size_t) i]) > tol)
            {
                ++mismatches;
                if (mismatches <= 8)
                    info ("mismatch " + prm->getName (32) + ": " + juce::String (prm->getValue(), 5) + " vs " + juce::String (values[(size_t) i], 5)
                          + " (steps " + juce::String (steps) + ")");
            }
        }
        check (mismatches == 0, "state restores all " + juce::String (values.size()) + " parameters (" + juce::String (mismatches) + " mismatches)");

        // automation sweep while processing. Delta and the bypass switches are held off: they
        // legitimately output unlimited audio (the difference signal / the untouched input).
        juce::AudioBuffer<float> buf (2, 64);
        juce::MidiBuffer midi;
        bool finite = true;
        auto* deltaParam = findParam (*p, "Delta");
        auto* bypassParam = findParam (*p, "Global Bypass");
        auto* maxBypassParam = findParam (*p, "Maximizer Bypass");
        for (int blk = 0; blk < 2000; ++blk)
        {
            fillMix (buf, fs, blk * 64);
            for (auto* prm : p->getParameters())
                if (rng.nextInt (20) == 0)
                    prm->setValueNotifyingHost (rng.nextFloat());
            setReal (deltaParam, 0.0f);
            setReal (bypassParam, 0.0f);
            setReal (maxBypassParam, 0.0f);
            p->processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 64; ++i)
                {
                    const bool ok = std::isfinite (buf.getSample (c, i)) && std::abs (buf.getSample (c, i)) <= 1.0001f;
                    if (! ok && finite)
                        info ("first bad sample at block " + juce::String (blk) + ": " + juce::String (buf.getSample (c, i)));
                    finite = finite && ok;
                }
        }
        check (finite, "random automation of every parameter keeps output finite and <= 0 dBFS");
    }

    // ---- 5. CPU ------------------------------------------------------------------------------
    std::printf ("[ RUN  ] CPU\n");
    {
        const double fs = 48000.0;
        auto p = load (fm, path, fs, 512);
        p->setPlayConfigDetails (2, 2, fs, 512);
        p->prepareToPlay (fs, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        const int blocks = (int) (fs * 10 / 512);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int blk = 0; blk < blocks; ++blk)
        {
            fillMix (buf, fs, blk * 512);
            p->processBlock (buf, midi);
        }
        const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        info ("default settings (4x, true peak): " + juce::String (10.0 / secs, 1) + "x realtime, " + juce::String (100.0 * secs / 10.0, 2) + " % of one core");
        check (secs < 10.0 / 3.0, "runs faster than 3x realtime");
    }

    // ---- 6. editor ---------------------------------------------------------------------------
    std::printf ("[ RUN  ] editor\n");
    {
        const double fs = 48000.0;
        auto p = load (fm, path, fs, 512);
        p->setPlayConfigDetails (2, 2, fs, 512);
        p->prepareToPlay (fs, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
        check (ed != nullptr, "editor opens");
        if (ed != nullptr)
        {
            // feed some audio so meters / analyzer / GR have content
            setReal (findParam (*p, "Threshold"), 0.7f);
            auto* b2 = findParam (*p, "Band 2 On");
            auto* b3 = findParam (*p, "Band 3 On");
            auto* b6 = findParam (*p, "Band 6 On");
            setReal (b2, 1.0f); setReal (b3, 1.0f); setReal (b6, 1.0f);
            setReal (findParam (*p, "Band 2 Gain"), 0.56f);
            setReal (findParam (*p, "Band 3 Gain"), 0.42f);
            setReal (findParam (*p, "Band 6 Gain"), 0.6f);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            juce::Random rng (5);
            for (int round = 0; round < 40; ++round)
            {
                for (int blk = 0; blk < 8; ++blk)
                {
                    fillMix (buf, fs, (round * 8 + blk) * 512);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 512; ++i)
                            buf.addSample (c, i, (rng.nextFloat() - 0.5f) * 0.1f);
                    p->processBlock (buf, midi);
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            }
            for (int w : { 826, 1180, 1770 })
            {
                ed->setSize (w, w * 720 / 1180);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                check (ed->getWidth() > 0, "editor resizes to " + juce::String (w));
            }
            ed->setSize (1180, 720);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            ed.reset();
        }
    }

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
