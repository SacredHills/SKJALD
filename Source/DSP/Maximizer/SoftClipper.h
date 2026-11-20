#pragma once

#include <cmath>

namespace skjald
{
/** Soft clipper: linear below knee * level, tanh saturation above, asymptotically
    approaching `level`. First derivative is continuous at the knee (no corner). */
inline float softClip (float x, float level, float knee) noexcept
{
    const float a = std::abs (x);
    const float k = knee * level;
    if (a <= k)
        return x;
    const float range = level - k;
    const float y = k + range * std::tanh ((a - k) / range);
    return std::copysign (y, x);
}

} // namespace skjald
