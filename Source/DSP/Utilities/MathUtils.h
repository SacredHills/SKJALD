#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skjald
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }

inline float gainToDb (float g, float floorDb = -200.0f) noexcept
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

inline double gainToDb (double g, double floorDb = -200.0) noexcept
{
    return g > 0.0 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb;
}

template <typename T>
constexpr T lerp (T a, T b, T t) noexcept { return a + (b - a) * t; }

template <typename T>
constexpr T clamp01 (T x) noexcept { return std::clamp (x, T (0), T (1)); }

/** One-pole smoothing coefficient for a time constant in seconds (63% rise time). */
inline double onePoleCoeff (double timeSeconds, double sampleRate) noexcept
{
    if (timeSeconds <= 0.0 || sampleRate <= 0.0)
        return 0.0;
    return std::exp (-1.0 / (timeSeconds * sampleRate));
}

inline int msToSamples (double ms, double sampleRate) noexcept
{
    return (int) std::lround (ms * 0.001 * sampleRate);
}

/** Modified Bessel function of the first kind, order 0 (for Kaiser windows). */
inline double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;
    const double halfX = 0.5 * x;
    for (int k = 1; k < 64; ++k)
    {
        term *= (halfX / k) * (halfX / k);
        sum += term;
        if (term < 1.0e-14 * sum)
            break;
    }
    return sum;
}

/** Kaiser window value for tap n of an N-tap window. */
inline double kaiserWindow (int n, int N, double beta) noexcept
{
    if (N <= 1)
        return 1.0;
    const double r = 2.0 * n / (N - 1) - 1.0;
    return besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
}

/** Kaiser beta for a desired stopband attenuation in dB. */
inline double kaiserBeta (double attenuationDb) noexcept
{
    if (attenuationDb > 50.0)
        return 0.1102 * (attenuationDb - 8.7);
    if (attenuationDb >= 21.0)
        return 0.5842 * std::pow (attenuationDb - 21.0, 0.4) + 0.07886 * (attenuationDb - 21.0);
    return 0.0;
}

inline double sinc (double x) noexcept
{
    if (std::abs (x) < 1.0e-12)
        return 1.0;
    const double px = kPi * x;
    return std::sin (px) / px;
}

inline bool isPowerOfTwo (int x) noexcept { return x > 0 && (x & (x - 1)) == 0; }

} // namespace skjald
