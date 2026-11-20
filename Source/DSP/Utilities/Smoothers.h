#pragma once

#include <cmath>
#include <algorithm>

namespace skjald
{
/** Linear ramp smoother: reaches the target in a fixed time. Allocation-free. */
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        countdown = 0;
        current = target;
    }

    void setCurrentAndTarget (float v) noexcept
    {
        current = target = v;
        countdown = 0;
    }

    void setTarget (float v) noexcept
    {
        if (! (v < target) && ! (v > target)) // unchanged (exact comparison intended)
            return;
        target = v;
        countdown = rampLength;
        step = (target - current) / (float) rampLength;
    }

    float next() noexcept
    {
        if (countdown <= 0)
            return current;
        if (--countdown == 0)
            current = target;
        else
            current += step;
        return current;
    }

    /** Advance by n samples and return the value reached. */
    float skip (int n) noexcept
    {
        if (countdown <= 0)
            return current;
        if (n >= countdown)
        {
            countdown = 0;
            current = target;
        }
        else
        {
            countdown -= n;
            current += step * (float) n;
        }
        return current;
    }

    bool isSmoothing() const noexcept { return countdown > 0; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int countdown = 0, rampLength = 1;
};

/** Gain smoother that interpolates in the dB domain (perceptually even ramps). */
class DecibelGainSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept { db.reset (sampleRate, rampSeconds); }
    void setCurrentAndTargetDb (float v) noexcept { db.setCurrentAndTarget (v); gain = std::pow (10.0f, v * 0.05f); }
    void setTargetDb (float v) noexcept { db.setTarget (v); }

    float next() noexcept
    {
        if (db.isSmoothing())
            gain = std::pow (10.0f, db.next() * 0.05f);
        return gain;
    }

    bool isSmoothing() const noexcept { return db.isSmoothing(); }
    float getCurrentGain() const noexcept { return gain; }

private:
    LinearSmoother db;
    float gain = 1.0f;
};

} // namespace skjald
