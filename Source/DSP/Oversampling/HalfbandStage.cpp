#include "HalfbandStage.h"
#include "../Utilities/MathUtils.h"

#include <cmath>
#include <algorithm>

namespace skjald
{
void HalfbandStage::design (double transitionWidth, double attenuationDb)
{
    // Kaiser length estimate, rounded up to the halfband form N = 4K - 1
    const double nEst = (attenuationDb - 8.0) / (2.285 * kTwoPi * std::max (1.0e-3, transitionWidth)) + 1.0;
    K = std::max (2, (int) std::ceil ((nEst + 1.0) / 4.0));
    numTaps = 4 * K - 1;
    M = 2 * K;
    const int c = 2 * K - 1;
    const double beta = kaiserBeta (attenuationDb);

    taps.assign ((size_t) M, 0.0f);
    std::vector<double> h ((size_t) M);
    double sum = 0.0;
    for (int j = 0; j < M; ++j)
    {
        const int k = 2 * j;             // even indices are the non-zero sinc taps (k - c is odd)
        const double x = 0.5 * (k - c);  // sinc argument for a cutoff at a quarter of the high rate
        h[(size_t) j] = 0.5 * sinc (x) * kaiserWindow (k, numTaps, beta);
        sum += h[(size_t) j];
    }
    // Normalise so the off-centre taps sum to exactly 0.5 (DC gain 1, halfband property kept)
    tapsUp.assign ((size_t) M, 0.0f);
    for (int j = 0; j < M / 2; ++j)
        h[(size_t) (M - 1 - j)] = h[(size_t) j]; // enforce exact symmetry for the folded loops
    for (int j = 0; j < M; ++j)
    {
        const double v = h[(size_t) j] * (0.5 / sum);
        taps[(size_t) j] = (float) v;
        tapsUp[(size_t) j] = (float) (2.0 * v);
    }

    upHist.assign ((size_t) (2 * M), 0.0f);
    downHist.assign ((size_t) (2 * M), 0.0f);
    oddDelay.prepare (K);
    reset();
}

void HalfbandStage::reset() noexcept
{
    std::fill (upHist.begin(), upHist.end(), 0.0f);
    std::fill (downHist.begin(), downHist.end(), 0.0f);
    oddDelay.reset();
    upPos = 0;
    downPos = 0;
}

void HalfbandStage::upsample (const float* in, float* out, int n) noexcept
{
    const float* t = tapsUp.data();
    for (int i = 0; i < n; ++i)
    {
        upPos = (upPos == 0 ? M : upPos) - 1;
        upHist[(size_t) upPos] = upHist[(size_t) (upPos + M)] = in[i];
        const float* h = upHist.data() + upPos; // h[j] = x[m - j]

        // linear-phase taps are symmetric (t[j] == t[M-1-j]): fold to halve the multiplies
        float acc = 0.0f;
        for (int j = 0; j < K; ++j)
            acc += t[j] * (h[j] + h[M - 1 - j]);

        out[2 * i] = acc;
        out[2 * i + 1] = h[K - 1]; // centre tap: 2 * 0.5 * x[m - (K - 1)]
    }
}

void HalfbandStage::downsample (const float* in, float* out, int n) noexcept
{
    const float* t = taps.data();
    for (int i = 0; i < n; ++i)
    {
        const float even = in[2 * i];
        const float odd = in[2 * i + 1];

        downPos = (downPos == 0 ? M : downPos) - 1;
        downHist[(size_t) downPos] = downHist[(size_t) (downPos + M)] = even;
        const float* h = downHist.data() + downPos;

        float acc = 0.0f;
        for (int j = 0; j < K; ++j)
            acc += t[j] * (h[j] + h[M - 1 - j]);

        // centre tap uses the odd sample from K pairs ago: u[2m - c] = u[2(m-K) + 1]
        acc += 0.5f * oddDelay.process (odd, K);
        out[i] = acc;
    }
}

} // namespace skjald
