#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../../Plugin/AudioTaps.h"

#include <array>
#include <vector>

namespace skjald::ui
{
/** GUI-thread spectrum analyser. Pulls samples from a lock-free FIFO (never touches the
    audio thread), runs an 8192-point Hann-windowed FFT with 75 % overlap and reduces it to
    log-spaced display points with fast-attack / slow-release smoothing and a +3 dB/oct
    display tilt (makes typical music look roughly flat). */
class SpectrumAnalyzer
{
public:
    static constexpr int kOrder = 13;
    static constexpr int kSize = 1 << kOrder;
    static constexpr int kPoints = 320;
    static constexpr float kMinFreq = 20.0f, kMaxFreq = 20000.0f;
    static constexpr float kFloorDb = -100.0f;

    SpectrumAnalyzer();

    void setSampleRate (double fs) noexcept;
    /** Returns true when new data was analysed. */
    bool update (SampleFifo& fifo) noexcept;
    void clear() noexcept;

    const std::array<float, kPoints>& getLevels() const noexcept { return levels; }
    static float pointFrequency (int i) noexcept;

private:
    void analyse() noexcept;

    juce::dsp::FFT fft { kOrder };
    std::vector<float> window, ring, fftData, scratch;
    int ringPos = 0, sinceLast = 0;
    double sampleRate = 48000.0;
    std::array<float, kPoints> levels {};
    float windowGain = 1.0f;
};

} // namespace skjald::ui
