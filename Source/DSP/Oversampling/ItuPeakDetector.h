#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace skjald
{
/** The ITU-R BS.1770-4 Annex 2 reference true-peak meter (4x oversampling, 48-tap polyphase
    interpolation filter with the coefficients published in the Recommendation).

    Delivery platforms and many loudness meters measure true peak with exactly this filter.
    Its passband ripple makes it read up to about 0.1 dB above the ideal reconstruction for
    some high-frequency material, so the output guard includes it: the output is then compliant
    both with an ideal reconstruction and with the standard meter.

    For each input sample it returns max |y| over the four phases, delayed by kLatency samples. */
class ItuPeakDetector
{
public:
    static constexpr int kTaps = 12;
    static constexpr int kLatency = 6;

    void reset() noexcept
    {
        hist.fill (0.0f);
        pos = 0;
    }

    float processPeak (float x) noexcept
    {
        hist[(size_t) pos] = x;
        hist[(size_t) (pos + kTaps)] = x;
        const float* w = hist.data() + pos + 1; // w[kTaps - 1] newest: y[n] = sum_j P[j] x[n - j]
        float m = 0.0f;
        for (const auto& ph : kPhases)
        {
            float acc = 0.0f;
            for (int j = 0; j < kTaps; ++j)
                acc += ph[(size_t) j] * w[kTaps - 1 - j];
            m = std::max (m, std::abs (acc));
        }
        if (++pos == kTaps)
            pos = 0;
        return m;
    }

private:
    static constexpr std::array<std::array<float, kTaps>, 4> kPhases { {
        { 0.0017089843750f, 0.0109863281250f, -0.0196533203125f, 0.0332031250000f, -0.0594482421875f, 0.1373291015625f,
          0.9721679687500f, -0.1022949218750f, 0.0476074218750f, -0.0266113281250f, 0.0148925781250f, -0.0083007812500f },
        { -0.0291748046875f, 0.0292968750000f, -0.0517578125000f, 0.0891113281250f, -0.1665039062500f, 0.4650878906250f,
          0.7797851562500f, -0.2003173828125f, 0.1015625000000f, -0.0582275390625f, 0.0330810546875f, -0.0189208984375f },
        { -0.0189208984375f, 0.0330810546875f, -0.0582275390625f, 0.1015625000000f, -0.2003173828125f, 0.7797851562500f,
          0.4650878906250f, -0.1665039062500f, 0.0891113281250f, -0.0517578125000f, 0.0292968750000f, -0.0291748046875f },
        { -0.0083007812500f, 0.0148925781250f, -0.0266113281250f, 0.0476074218750f, -0.1022949218750f, 0.9721679687500f,
          0.1373291015625f, -0.0594482421875f, 0.0332031250000f, -0.0196533203125f, 0.0109863281250f, 0.0017089843750f },
    } };

    std::array<float, 2 * kTaps> hist {};
    int pos = 0;
};

} // namespace skjald
