#include "TransientDetector.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void TransientDetector::prepare (double sampleRate) noexcept
{
    fastAtt = onePoleCoeff (0.0002, sampleRate);
    slowAtt = onePoleCoeff (0.010, sampleRate);
    rel = onePoleCoeff (0.060, sampleRate);
    reset();
}

void TransientDetector::reset() noexcept
{
    fast = slow = 0.0;
}

void TransientDetector::process (const float* const* in, int numChannels, int numSamples, float* attackness) noexcept
{
    numChannels = std::clamp (numChannels, 1, 2);
    for (int i = 0; i < numSamples; ++i)
    {
        double p = std::abs (in[0][i]);
        if (numChannels > 1)
            p = std::max (p, (double) std::abs (in[1][i]));
        fast = p + (fast - p) * (p > fast ? fastAtt : rel);
        slow = p + (slow - p) * (p > slow ? slowAtt : rel);
        const double ratioDb = 20.0 * std::log10 ((fast + 1.0e-9) / (slow + 1.0e-9));
        attackness[i] = (float) clamp01 (ratioDb / kFullScaleDb);
    }
}

} // namespace skjald
