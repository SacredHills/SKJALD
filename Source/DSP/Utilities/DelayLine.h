#pragma once

#include <vector>
#include <algorithm>

namespace skjald
{
/** Integer-sample circular delay line. Memory is allocated in prepare() only. */
template <typename T = float>
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        size = std::max (1, maxDelaySamples + 1);
        buffer.assign ((size_t) size, T (0));
        writePos = 0;
    }

    void reset (T value = T (0)) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), value);
        writePos = 0;
    }

    /** Push x and return the sample written `delay` samples ago (0 <= delay <= maxDelay). */
    T process (T x, int delay) noexcept
    {
        buffer[(size_t) writePos] = x;
        int r = writePos - delay;
        if (r < 0)
            r += size;
        if (++writePos == size)
            writePos = 0;
        return buffer[(size_t) r];
    }

    int getMaxDelay() const noexcept { return size - 1; }

private:
    std::vector<T> buffer;
    int size = 1, writePos = 0;
};

} // namespace skjald
