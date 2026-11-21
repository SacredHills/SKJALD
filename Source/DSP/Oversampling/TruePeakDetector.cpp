#include "TruePeakDetector.h"

#include <algorithm>
#include <cmath>

namespace skjald
{
void TruePeakDetector::prepare (int factor, double attenuationDb, double edge, bool refinePeaks)
{
    refine = refinePeaks;
    stages = factor >= 8 ? 3 : (factor >= 4 ? 2 : 1);

    // The first stage's transition band is centred on fs/2: (1 - edge) .. edge
    edge = std::clamp (edge, 0.501, 0.75);
    long long totalTop = 0;
    for (int s = 1; s <= stages; ++s)
    {
        const double outRel = (double) (1 << s), inRel = (double) (1 << (s - 1));
        const double width = (s == 1) ? (2.0 * (edge - 0.5)) / outRel : (inRel - 2.0 * edge) / outRel;
        filters[(size_t) (s - 1)].design (width, attenuationDb);
        // interpolation delay of stage s: centre samples at its output rate
        totalTop += (long long) filters[(size_t) (s - 1)].getCentre() << (stages - s);
    }
    const int F = 1 << stages;
    padTop = (int) ((F - (totalTop % F)) % F);
    latency = (int) ((totalTop + padTop) / F) + (refine ? 1 : 0);

    for (int s = 0; s <= stages; ++s)
        work[(size_t) s].assign ((size_t) (kMaxBlock << s), 0.0f);
    pad.prepare (std::max (1, padTop));
    reset();
}

void TruePeakDetector::reset() noexcept
{
    for (auto& f : filters)
        f.reset();
    pad.reset();
    prevPrevTop = prevTop = pendingPeak = 0.0f;
}

void TruePeakDetector::process (const float* in, float* peaks, int n) noexcept
{
    const int F = 1 << stages;
    for (int start = 0; start < n; start += kMaxBlock)
    {
        const int len = std::min (kMaxBlock, n - start);
        std::copy (in + start, in + start + len, work[0].begin());
        for (int s = 1; s <= stages; ++s)
            filters[(size_t) (s - 1)].upsample (work[(size_t) (s - 1)].data(), work[(size_t) s].data(), len << (s - 1));

        float* top = work[(size_t) stages].data();
        if (padTop > 0)
            for (int i = 0; i < len * F; ++i)
                top[i] = pad.process (top[i], padTop);

        if (! refine)
        {
            for (int i = 0; i < len; ++i)
            {
                float m = 0.0f;
                const float* p = top + i * F;
                for (int k = 0; k < F; ++k)
                    m = std::max (m, std::abs (p[k]));
                peaks[start + i] = m;
            }
            continue;
        }

        // Refined: the oversampled point y[t-1] is evaluated when y[t] arrives. A point that is a
        // local maximum of |y| gets the parabolic vertex through (y[t-2], y[t-1], y[t]). Points of
        // sample period i are complete once the first point of period i + 1 is known, hence the
        // extra sample of latency: the value of period i is emitted with period i + 1.
        for (int i = 0; i < len; ++i)
        {
            const float* p = top + i * F;
            float m = pendingPeak; // the previous period's maximum still lacks its last point
            float cur = 0.0f;
            for (int k = 0; k < F; ++k)
            {
                const float a = prevPrevTop, b = prevTop, c = p[k];
                float v = std::abs (b);
                const float s = b >= 0.0f ? 1.0f : -1.0f;
                const float sa = s * a, sb = s * b, sc = s * c;
                if (sb >= sa && sb >= sc)
                {
                    const float den = sa - 2.0f * sb + sc;
                    if (den < 0.0f)
                    {
                        const float off = 0.5f * (sa - sc) / den; // within +-0.5 for a local max
                        v = sb - 0.25f * (sa - sc) * off;
                    }
                }
                // point b belongs to the previous period when k == 0, to this period otherwise
                if (k == 0)
                    m = std::max (m, v);
                else
                    cur = std::max (cur, v);
                prevPrevTop = prevTop;
                prevTop = c;
            }
            peaks[start + i] = m;
            pendingPeak = cur;
        }
    }
}

} // namespace skjald
