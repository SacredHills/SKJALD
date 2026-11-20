#include "PluginEditor.h"

namespace skjald
{
SkjaldAudioProcessorEditor::SkjaldAudioProcessorEditor (SkjaldAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel); // popup menus / alert windows
    main = std::make_unique<ui::MainWindow> (p);
    addAndMakeVisible (*main);

    constexpr int w = ui::MainWindow::kDesignWidth, h = ui::MainWindow::kDesignHeight;
    setResizable (true, true);
    setResizeLimits (w * 7 / 10, h * 7 / 10, w * 2, h * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) w / (double) h);

    const int savedW = (int) processor.getUIState().getProperty ("width", w);
    const int savedH = (int) processor.getUIState().getProperty ("height", h);
    setSize (juce::jlimit (w * 7 / 10, w * 2, savedW), juce::jlimit (h * 7 / 10, h * 2, savedH));

    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

SkjaldAudioProcessorEditor::~SkjaldAudioProcessorEditor()
{
    stopTimer();
    main.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SkjaldAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);
}

void SkjaldAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::MainWindow::kDesignWidth;
    main->setTransform (juce::AffineTransform::scale (scale));
    main->setBounds (0, 0, ui::MainWindow::kDesignWidth, ui::MainWindow::kDesignHeight);
    processor.getUIState().setProperty ("width", getWidth(), nullptr);
    processor.getUIState().setProperty ("height", getHeight(), nullptr);
}

void SkjaldAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.25, (now - lastTime) * 0.001);
    lastTime = now;
    main->refresh (dt, frame++);
}

} // namespace skjald
