#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
double LoudnessMeter::energyToLufs (double e) noexcept
{
    if (e <= 1.0e-20)
        return kSilenceLufs;
    return std::max ((double) kSilenceLufs, -0.691 + 10.0 * std::log10 (e));
}

void LoudnessMeter::prepare (double sampleRate)
{
    for (auto& f : filters)
        f.prepare (sampleRate);
    subBlockLength = std::max (1, (int) std::lround (sampleRate * 0.1));
    histCount.assign ((size_t) kHistBins, 0u);
    histEnergy.assign ((size_t) kHistBins, 0.0);
    reset();
}

void LoudnessMeter::reset() noexcept
{
    for (auto& f : filters)
        f.reset();
    subBlockPos = 0;
    subBlockSum = 0.0;
    subBlocks.fill (0.0);
    subBlockCount = 0;
    subBlockWrite = 0;
    momentary = kSilenceLufs;
    shortTerm = kSilenceLufs;
    integratedResetRequest = false;
    std::fill (histCount.begin(), histCount.end(), 0u);
    std::fill (histEnergy.begin(), histEnergy.end(), 0.0);
    gatedTotalEnergy = 0.0;
    gatedTotalCount = 0;
    integrated = kSilenceLufs;
}

void LoudnessMeter::resetIntegrated() noexcept
{
    // Executed by the audio thread at its next block (thread-safe request)
    integratedResetRequest.store (true, std::memory_order_release);
}

void LoudnessMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (integratedResetRequest.exchange (false, std::memory_order_acq_rel))
    {
        std::fill (histCount.begin(), histCount.end(), 0u);
        std::fill (histEnergy.begin(), histEnergy.end(), 0.0);
        gatedTotalEnergy = 0.0;
        gatedTotalCount = 0;
        integrated.store (kSilenceLufs, std::memory_order_relaxed);
    }

    numChannels = std::clamp (numChannels, 1, kMaxChannels);
    int i = 0;
    while (i < numSamples)
    {
        const int len = std::min (numSamples - i, subBlockLength - subBlockPos);
        for (int c = 0; c < numChannels; ++c)
        {
            auto& f = filters[(size_t) c];
            const float* x = channels[c] + i;
            double acc = 0.0;
            for (int k = 0; k < len; ++k)
            {
                const double z = f.process ((double) x[k]);
                acc += z * z;
            }
            subBlockSum += acc;
        }
        subBlockPos += len;
        i += len;
        if (subBlockPos >= subBlockLength)
            finishSubBlock();
    }
}

void LoudnessMeter::finishSubBlock() noexcept
{
    const double energy = subBlockSum / subBlockLength; // sum over channels of mean square
    subBlockSum = 0.0;
    subBlockPos = 0;

    subBlocks[(size_t) subBlockWrite] = energy;
    subBlockWrite = (subBlockWrite + 1) % (int) subBlocks.size();
    subBlockCount = std::min (subBlockCount + 1, 1 << 30);

    auto meanOfLast = [this] (int n)
    {
        double s = 0.0;
        int idx = subBlockWrite;
        for (int k = 0; k < n; ++k)
        {
            idx = (idx == 0 ? (int) subBlocks.size() : idx) - 1;
            s += subBlocks[(size_t) idx];
        }
        return s / n;
    };

    const int available = std::min (subBlockCount, (int) subBlocks.size());
    if (available >= 4)
    {
        const double blockEnergy = meanOfLast (4);
        momentary.store ((float) energyToLufs (blockEnergy), std::memory_order_relaxed);

        // Integrated: every 100 ms a new 400 ms block (75 % overlap)
        const double l = energyToLufs (blockEnergy);
        if (l >= kHistMin)
        {
            const int bin = std::clamp ((int) ((l - kHistMin) / kHistStep), 0, kHistBins - 1);
            histCount[(size_t) bin] += 1;
            histEnergy[(size_t) bin] += blockEnergy;
            gatedTotalEnergy += blockEnergy;
            gatedTotalCount += 1;
            updateIntegrated();
        }
    }
    else
    {
        momentary.store ((float) energyToLufs (meanOfLast (available)), std::memory_order_relaxed);
    }

    shortTerm.store ((float) energyToLufs (meanOfLast (std::max (1, available))), std::memory_order_relaxed);
}

void LoudnessMeter::updateIntegrated() noexcept
{
    if (gatedTotalCount == 0)
        return;
    const double relativeGate = energyToLufs (gatedTotalEnergy / (double) gatedTotalCount) - 10.0;
    const int startBin = std::clamp ((int) std::ceil ((relativeGate - kHistMin) / kHistStep), 0, kHistBins - 1);

    double e = 0.0;
    uint64_t n = 0;
    for (int b = startBin; b < kHistBins; ++b)
    {
        e += histEnergy[(size_t) b];
        n += histCount[(size_t) b];
    }
    integrated.store (n > 0 ? (float) energyToLufs (e / (double) n) : kSilenceLufs, std::memory_order_relaxed);
}

} // namespace skjald
