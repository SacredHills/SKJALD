#include "PeakInterpolator.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void PeakInterpolator::prepare (int factor, int halfLengthInputSamples, double attenuationDb)
{
    F = std::max (1, factor);
    D = F > 1 ? std::max (1, halfLengthInputSamples) : 0;
    J = 2 * D + 1;
    coeffs.assign ((size_t) (F * J), 0.0f);

    if (F > 1)
    {
        const int L = 2 * D * F + 1;
        const int centre = D * F;
        const double beta = kaiserBeta (attenuationDb);
        for (int p = 0; p < F; ++p)
        {
            double sum = 0.0;
            for (int j = 0; j < J; ++j)
            {
                const int k = p + j * F;
                if (k >= L)
                    continue;
                const double v = sinc ((double) (k - centre) / F) * kaiserWindow (k, L, beta);
                coeffs[(size_t) (p * J + j)] = (float) v;
                sum += v;
            }
            // unity DC gain per phase
            for (int j = 0; j < J; ++j)
                coeffs[(size_t) (p * J + j)] = (float) (coeffs[(size_t) (p * J + j)] / sum);
        }
    }
    else
    {
        coeffs[0] = 1.0f;
    }

    hist.assign ((size_t) (2 * J), 0.0f);
    reset();
}

void PeakInterpolator::reset() noexcept
{
    std::fill (hist.begin(), hist.end(), 0.0f);
    pos = 0;
}

float PeakInterpolator::processPeak (float x) noexcept
{
    if (F == 1)
        return std::abs (x);

    pos = (pos == 0 ? J : pos) - 1;
    hist[(size_t) pos] = hist[(size_t) (pos + J)] = x;
    const float* h = hist.data() + pos; // h[j] = x[n - j]

    float peak = 0.0f;
    for (int p = 0; p < F; ++p)
    {
        const float* c = coeffs.data() + p * J;
        float acc = 0.0f;
        for (int j = 0; j < J; ++j)
            acc += c[j] * h[j];
        peak = std::max (peak, std::abs (acc));
    }
    return peak;
}

} // namespace skjald
