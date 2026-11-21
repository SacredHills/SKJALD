#include "TransientDetector.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void TransientDetector::prepare (double sampleRate) noexcept
{
    fastCoeff = onePoleCoeff (0.0005, sampleRate);
    holdAttack = onePoleCoeff (0.008, sampleRate);
    holdRelease = onePoleCoeff (0.080, sampleRate);
    weightRelease = onePoleCoeff (0.004, sampleRate);
    reset();
}

void TransientDetector::reset() noexcept
{
    fast = hold = 0.0;
    weight = 0.0;
}

void TransientDetector::process (const float* const* in, int numChannels, int numSamples, float* attackness) noexcept
{
    numChannels = std::clamp (numChannels, 1, 2);
    for (int i = 0; i < numSamples; ++i)
    {
        double p = std::abs (in[0][i]);
        if (numChannels > 1)
            p = std::max (p, (double) std::abs (in[1][i]));
        fast = p + (fast - p) * fastCoeff;
        hold = p + (hold - p) * (p > hold ? holdAttack : holdRelease);
        const double ratioDb = 20.0 * std::log10 ((fast + kNoiseFloor) / (hold + kNoiseFloor));
        // peak-hold the weight for a few ms: the limiter's gain is a minimum over its attack window,
        // so the whole stretch of attack has to be relaxed together to raise the gain at all
        const double w = clamp01 ((ratioDb - kDeadZoneDb) / kFullScaleDb);
        weight = w > weight ? w : w + (weight - w) * weightRelease;
        attackness[i] = (float) weight;
    }
}

} // namespace skjald
