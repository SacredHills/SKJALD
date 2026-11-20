#include "SpectrumAnalyzer.h"

namespace skjald::ui
{
SpectrumAnalyzer::SpectrumAnalyzer()
    : window ((size_t) kSize), ring ((size_t) kSize, 0.0f), fftData ((size_t) kSize * 2, 0.0f), scratch (8192, 0.0f)
{
    double sum = 0.0;
    for (int i = 0; i < kSize; ++i)
    {
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1));
        sum += window[(size_t) i];
    }
    windowGain = (float) (2.0 / sum); // a full-scale sine reads 0 dB
    clear();
}

void SpectrumAnalyzer::setSampleRate (double fs) noexcept
{
    if (fs > 0.0)
        sampleRate = fs;
}

void SpectrumAnalyzer::clear() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    levels.fill (kFloorDb);
    ringPos = 0;
    sinceLast = 0;
}

float SpectrumAnalyzer::pointFrequency (int i) noexcept
{
    return kMinFreq * std::pow (kMaxFreq / kMinFreq, (float) i / (float) (kPoints - 1));
}

bool SpectrumAnalyzer::update (SampleFifo& fifo) noexcept
{
    bool analysed = false;
    for (;;)
    {
        const int n = fifo.pull (scratch.data(), (int) scratch.size());
        if (n <= 0)
            break;
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = scratch[(size_t) i];
            ringPos = (ringPos + 1) & (kSize - 1);
        }
        sinceLast += n;
        if (sinceLast >= kSize / 4)
        {
            sinceLast = 0;
            analyse();
            analysed = true;
        }
    }
    return analysed;
}

void SpectrumAnalyzer::analyse() noexcept
{
    for (int i = 0; i < kSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) & (kSize - 1))] * window[(size_t) i];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const float binHz = (float) (sampleRate / kSize);
    for (int p = 0; p < kPoints; ++p)
    {
        // band edges halfway (in log terms) to the neighbouring display points
        const float f = pointFrequency (p);
        const float ratio = std::pow (kMaxFreq / kMinFreq, 0.5f / (float) (kPoints - 1));
        int b0 = (int) std::floor (f / ratio / binHz);
        int b1 = (int) std::ceil (f * ratio / binHz);
        b0 = juce::jlimit (1, kSize / 2 - 1, b0);
        b1 = juce::jlimit (b0, kSize / 2 - 1, b1);

        float mag = 0.0f;
        if (b1 - b0 <= 1)
        {
            // interpolate between bins at low frequencies
            const float pos = f / binHz;
            const int i0 = juce::jlimit (1, kSize / 2 - 2, (int) pos);
            const float frac = juce::jlimit (0.0f, 1.0f, pos - (float) i0);
            mag = fftData[(size_t) i0] * (1.0f - frac) + fftData[(size_t) i0 + 1] * frac;
        }
        else
        {
            for (int b = b0; b <= b1; ++b)
                mag = juce::jmax (mag, fftData[(size_t) b]);
        }

        float db = juce::Decibels::gainToDecibels (mag * windowGain, kFloorDb);
        db += 3.0f * std::log2 (f / 1000.0f); // display tilt
        auto& l = levels[(size_t) p];
        l = db > l ? l + (db - l) * 0.6f : l + (db - l) * 0.12f;
        l = juce::jmax (kFloorDb, l);
    }
}

} // namespace skjald::ui
