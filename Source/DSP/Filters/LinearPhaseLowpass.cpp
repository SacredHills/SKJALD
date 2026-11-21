#include "LinearPhaseLowpass.h"
#include "../Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void LinearPhaseLowpass::design (double passEdge, double stopEdge, double attenuationDb)
{
    passEdge = std::clamp (passEdge, 0.01, 0.49);
    stopEdge = std::clamp (stopEdge, passEdge + 0.001, 0.5);
    const double deltaOmega = 2.0 * kPi * (stopEdge - passEdge);
    int n = (int) std::ceil ((attenuationDb - 7.95) / (2.285 * deltaOmega)) + 1;
    n |= 1; // odd
    half = (n - 1) / 2;
    N = n;

    const double fc = 0.5 * (passEdge + stopEdge);
    const double beta = kaiserBeta (attenuationDb);
    std::vector<double> h ((size_t) half + 1);
    double sum = 0.0;
    for (int k = 0; k <= half; ++k)
    {
        h[(size_t) k] = 2.0 * fc * sinc (2.0 * fc * k) * kaiserWindow (half + k, N, beta);
        sum += (k == 0 ? 1.0 : 2.0) * h[(size_t) k];
    }
    taps.resize ((size_t) half + 1);
    for (int k = 0; k <= half; ++k)
        taps[(size_t) k] = (float) (h[(size_t) k] / sum); // unity DC gain

    hist.assign ((size_t) (2 * N), 0.0f);
    reset();
}

void LinearPhaseLowpass::reset() noexcept
{
    std::fill (hist.begin(), hist.end(), 0.0f);
    pos = 0;
}

float LinearPhaseLowpass::process (float x) noexcept
{
    // double-written history: hist[pos] and hist[pos + N] hold the same sample, so the
    // newest N samples are always contiguous at hist[pos + 1 .. pos + N]
    hist[(size_t) pos] = x;
    hist[(size_t) (pos + N)] = x;
    const float* w = hist.data() + pos + 1; // w[0] oldest ... w[N - 1] newest
    float acc = taps[0] * w[half];
    for (int k = 1; k <= half; ++k)
        acc += taps[(size_t) k] * (w[half - k] + w[half + k]);
    if (++pos == N)
        pos = 0;
    return acc;
}

double LinearPhaseLowpass::magnitudeAt (double f) const noexcept
{
    double re = taps.empty() ? 0.0 : taps[0];
    for (int k = 1; k <= half; ++k)
        re += 2.0 * taps[(size_t) k] * std::cos (2.0 * kPi * f * k);
    return std::abs (re);
}

} // namespace skjald
