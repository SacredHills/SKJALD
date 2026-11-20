#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>

namespace skjald
{
/** Running minimum over the last L samples (monotonic deque, O(1) amortised). */
class SlidingMin
{
public:
    void prepare (int maxLength)
    {
        capacity = std::max (1, maxLength) + 1;
        values.assign ((size_t) capacity, 1.0f);
        indices.assign ((size_t) capacity, 0);
        reset();
    }

    void reset() noexcept
    {
        head = tail = count = 0;
        n = 0;
    }

    void setLength (int L) noexcept { length = std::clamp (L, 1, capacity - 1); }
    int getLength() const noexcept { return length; }

    float process (float x) noexcept
    {
        // drop dominated values from the back
        while (count > 0)
        {
            const int back = (tail == 0 ? capacity : tail) - 1;
            if (values[(size_t) back] < x)
                break;
            tail = back;
            --count;
        }
        values[(size_t) tail] = x;
        indices[(size_t) tail] = n;
        tail = (tail + 1 == capacity) ? 0 : tail + 1;
        ++count;

        // expire values that left the window
        while (count > 0 && indices[(size_t) head] <= n - length)
        {
            head = (head + 1 == capacity) ? 0 : head + 1;
            --count;
        }
        ++n;
        return values[(size_t) head];
    }

private:
    std::vector<float> values;
    std::vector<int64_t> indices;
    int capacity = 2, head = 0, tail = 0, count = 0, length = 1;
    int64_t n = 0;
};

/** Moving average of the last L samples with a double-precision running sum. */
class BoxFilter
{
public:
    void prepare (int maxLength)
    {
        capacity = std::max (1, maxLength) + 1; // one spare slot for stepTowards()
        history.assign ((size_t) capacity, 1.0f);
        length = 1;
        inv = 1.0;
        reset (1.0f);
    }

    void reset (float value) noexcept
    {
        std::fill (history.begin(), history.end(), value);
        pos = 0;
        sum = (double) value * length;
    }

    void setLength (int L) noexcept
    {
        L = std::clamp (L, 1, capacity - 1);
        if (L == length)
            return;
        length = L;
        // recompute the running sum over the new window from the stored history
        double s = 0.0;
        int p = pos;
        for (int i = 0; i < length; ++i)
        {
            p = (p == 0 ? capacity : p) - 1;
            s += history[(size_t) p];
        }
        sum = s;
        inv = 1.0 / length;
    }

    int getLength() const noexcept { return length; }

    /** O(1) length change by one sample (used to slew the attack window). */
    void stepTowards (int target) noexcept
    {
        target = std::clamp (target, 1, capacity - 1);
        if (target > length)
        {
            // include the sample just older than the current window
            int older = pos - length - 1;
            while (older < 0)
                older += capacity;
            sum += history[(size_t) older];
            ++length;
        }
        else if (target < length)
        {
            int oldest = pos - length;
            if (oldest < 0)
                oldest += capacity;
            sum -= history[(size_t) oldest];
            --length;
        }
        inv = 1.0 / length;
    }

    float process (float x) noexcept
    {
        int oldest = pos - length;
        if (oldest < 0)
            oldest += capacity;
        sum += (double) x - (double) history[(size_t) oldest];
        history[(size_t) pos] = x;
        pos = (pos + 1 == capacity) ? 0 : pos + 1;
        return (float) (sum * inv);
    }

private:
    std::vector<float> history;
    int capacity = 1, pos = 0, length = 1;
    double sum = 1.0, inv = 1.0;
};

} // namespace skjald
