#include "Leveler.h"
#include "../Utilities/MathUtils.h"

#include <cmath>
#include <algorithm>

namespace skjald
{
void Leveler::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    // Two cascaded 25 ms one-poles on x^2: the squared signal of a 40 Hz bass tone ripples at
    // 80 Hz, and a single short integrator turns that ripple into gain modulation inside every
    // cycle (intermodulation). Cascaded, the ripple is ~44 dB down. The peak limiter, not the
    // leveler, catches fast events.
    msCoeff = onePoleCoeff (0.025, fs);
    reset();
}

void Leveler::reset() noexcept
{
    ms1[0] = ms1[1] = ms2[0] = ms2[1] = 0.0;
    grDb[0] = grDb[1] = 0.0;
    currentGrDb = 0.0f;
}

void Leveler::process (float* const* io, int numChannels, int numSamples, const LevelerConfig& cfg,
                       float ceilingDb, float link, float attackScale, float* grDbOut) noexcept
{
    numChannels = std::min (numChannels, 2);
    if (! cfg.enabled)
    {
        // Let any residual gain reduction recover smoothly rather than jump
        if (grDb[0] < 1.0e-4 && grDb[1] < 1.0e-4)
        {
            currentGrDb = 0.0f;
            if (grDbOut != nullptr)
                std::fill (grDbOut, grDbOut + numSamples, 0.0f);
            return;
        }
    }

    const double att = onePoleCoeff (cfg.attackMs * 0.001 * std::max (1.0f, attackScale), fs);
    const double rel = onePoleCoeff (cfg.releaseMs * 0.001, fs);
    const double threshold = ceilingDb - cfg.thresholdBelowCeilingDb;
    const double slope = 1.0 - 1.0 / std::max (1.0f, cfg.ratio);
    const double knee = std::max (0.01f, cfg.kneeDb);
    const double maxGr = cfg.maxGainReductionDb;
    const double ln = std::clamp ((double) link, 0.0, 1.0);

    double target[2] { 0.0, 0.0 };
    float maxGrSeen = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            const double x = io[c][i];
            ms1[c] = x * x + (ms1[c] - x * x) * msCoeff;
            ms2[c] = ms1[c] + (ms2[c] - ms1[c]) * msCoeff;

            double gr = 0.0;
            if (cfg.enabled)
            {
                const double level = 10.0 * std::log10 (ms2[c] + 1.0e-20);
                const double over = level - threshold;
                if (over > knee * 0.5)
                    gr = over * slope;
                else if (over > -knee * 0.5)
                {
                    const double k = over + knee * 0.5;
                    gr = slope * k * k / (2.0 * knee);
                }
                gr = std::min (gr, maxGr);
            }
            target[c] = gr;
        }

        if (numChannels == 2)
        {
            const double m = std::max (target[0], target[1]);
            target[0] += (m - target[0]) * ln;
            target[1] += (m - target[1]) * ln;
        }

        float sampleGr = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            const double coeff = target[c] > grDb[c] ? att : rel;
            grDb[c] = target[c] + (grDb[c] - target[c]) * coeff;
            const float g = (float) std::pow (10.0, -grDb[c] * 0.05);
            io[c][i] *= g;
            maxGrSeen = std::max (maxGrSeen, (float) grDb[c]);
            sampleGr = std::max (sampleGr, (float) grDb[c]);
        }
        if (grDbOut != nullptr)
            grDbOut[i] = sampleGr;
    }
    currentGrDb = maxGrSeen;
}

} // namespace skjald
