#pragma once

#include <algorithm>
#include <cmath>

namespace skjald
{
/** Learn Threshold (after iZotope's Maximizer): from the integrated loudness and the peak level of
    the incoming material, returns the threshold that brings the maximizer's output to about the
    target loudness.

    Drive into the limiter is inputGain - threshold. Pure gain would give inputLufs + drive, but the
    limiter removes the part of the peaks that pokes over the ceiling and with it some loudness:
        over = max(0, peak + drive - ceiling)
    and loss(over) is an empirical table (Modern mode, programme material). The equation
    inputLufs + drive - loss = target is solved by bisection. Result is clamped to the parameter range. */
inline float learnedThresholdDb (float inputLufs, float targetLufs, float inputGainDb,
                                 float inputPeakDb, float ceilingDb) noexcept
{
    if (inputLufs <= -69.0f)
        return 0.0f; // nothing measured: leave unity drive

    auto outputLufs = [&] (float drive)
    {
        const float over = std::max (0.0f, inputPeakDb + drive - ceilingDb);
        // (over, loss) measured on programme material, Modern mode; linear in between
        static constexpr float table[][2] = { { 0.0f, 0.0f }, { 3.7f, 1.9f }, { 6.7f, 3.97f }, { 11.5f, 7.1f }, { 20.5f, 14.5f } };
        constexpr int n = (int) (sizeof (table) / sizeof (table[0]));
        float loss = table[n - 1][1] + 0.85f * (over - table[n - 1][0]);
        for (int i = 1; i < n; ++i)
            if (over <= table[i][0])
            {
                const float t = (over - table[i - 1][0]) / (table[i][0] - table[i - 1][0]);
                loss = table[i - 1][1] + t * (table[i][1] - table[i - 1][1]);
                break;
            }
        return inputLufs + drive - loss;
    };

    float lo = 0.0f, hi = 40.0f; // drive range (output loudness is monotonic up to hi)
    for (int i = 0; i < 40; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        (outputLufs (mid) < targetLufs ? lo : hi) = mid;
    }
    const float t = std::clamp (inputGainDb - 0.5f * (lo + hi), -30.0f, 0.0f);
    return std::round (t * 100.0f) / 100.0f; // the parameter's own resolution
}
} // namespace skjald
