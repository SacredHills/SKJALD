#include "Oversampler.h"

#include <algorithm>

namespace skjald
{
void Oversampler::prepare (int numStages, int maxBaseBlock, double attenuationDb)
{
    stages = std::clamp (numStages, 0, kMaxStages);
    maxBlock = std::max (1, maxBaseBlock);

    // Passband edge and first image edge relative to the base rate
    constexpr double fp = 0.4535, fsb = 0.5465;

    long long totalTop = 0; // round-trip delay in top-rate samples
    for (int s = 1; s <= stages; ++s)
    {
        const double outRateRel = (double) (1 << s);              // output rate / base rate
        const double inRateRel = (double) (1 << (s - 1));
        const double width = (s == 1) ? (fsb - fp) / outRateRel
                                      : (inRateRel - 2.0 * fp) / outRateRel;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            filters[(size_t) ch][(size_t) (s - 1)].design (width, attenuationDb);

        const int c = filters[0][(size_t) (s - 1)].getCentre();
        totalTop += (long long) c << (stages - s + 1);
    }

    const int factor = 1 << stages;
    padTop = (int) ((factor - (totalTop % factor)) % factor);
    latencyBase = (int) ((totalTop + padTop) / factor);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        for (int s = 0; s <= stages; ++s)
            work[(size_t) ch][(size_t) s].assign ((size_t) (maxBlock << s), 0.0f);
        pad[(size_t) ch].prepare (std::max (1, padTop));
    }
    reset();
}

void Oversampler::reset() noexcept
{
    for (auto& chFilters : filters)
        for (auto& f : chFilters)
            f.reset();
    for (auto& p : pad)
        p.reset();
}

void Oversampler::upsample (const float* const* in, int numChannels, int n) noexcept
{
    n = std::min (n, maxBlock);
    for (int ch = 0; ch < std::min (numChannels, kMaxChannels); ++ch)
    {
        auto& w = work[(size_t) ch];
        std::copy (in[ch], in[ch] + n, w[0].begin());
        for (int s = 1; s <= stages; ++s)
            filters[(size_t) ch][(size_t) (s - 1)].upsample (w[(size_t) (s - 1)].data(), w[(size_t) s].data(), n << (s - 1));
    }
}

void Oversampler::downsample (float* const* out, int numChannels, int n) noexcept
{
    n = std::min (n, maxBlock);
    for (int ch = 0; ch < std::min (numChannels, kMaxChannels); ++ch)
    {
        auto& w = work[(size_t) ch];
        if (padTop > 0)
        {
            float* top = w[(size_t) stages].data();
            const int topN = n << stages;
            for (int i = 0; i < topN; ++i)
                top[i] = pad[(size_t) ch].process (top[i], padTop);
        }
        for (int s = stages; s >= 1; --s)
            filters[(size_t) ch][(size_t) (s - 1)].downsample (w[(size_t) s].data(), w[(size_t) (s - 1)].data(), n << (s - 1));
        std::copy (w[0].begin(), w[0].begin() + n, out[ch]);
    }
}

} // namespace skjald
