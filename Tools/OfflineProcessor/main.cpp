// skjald_offline: processes WAV files through the complete SKJALD chain (EQ -> Maximizer)
// and writes a measurement report. Uses exactly the same DSP engine as the plug-in.
//
//   skjald_offline input.wav output.wav [options]
//   skjald_offline --benchmark [seconds]
//   skjald_offline --help

#include "Engine/MasteringEngine.h"
#include "Metering/LoudnessMeter.h"
#include "Metering/TruePeakMeter.h"
#include "WavFile.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace skjald;

namespace
{
void usage()
{
    std::printf (
        "SKJALD offline processor\n\n"
        "usage: skjald_offline <input.wav> <output.wav> [options]\n"
        "       skjald_offline --benchmark [seconds]\n\n"
        "Maximizer options:\n"
        "  --mode <transparent|modern|aggressive|punch|smooth>   (default modern)\n"
        "  --threshold <dB>      -30..0   (default 0)\n"
        "  --ceiling <dB>        -12..0   (default -1)\n"
        "  --input-gain <dB>     -24..24  (default 0)\n"
        "  --release <ms>        5..1000  (default 100)\n"
        "  --auto-release\n"
        "  --character <0..10>   Fast..Slow overall speed (default 5)\n"
        "  --lookahead <ms>      0..10    (default 5)\n"
        "  --link <percent>      0..100   (default 100)\n"
        "  --link-mode <standard|split>\n"
        "  --transient <percent> 0..100   (default 0)\n"
        "  --os <1|2|4|8|16>     oversampling (default 4)\n"
        "  --no-true-peak        sample-peak ceiling instead of true peak\n"
        "  --no-adaptive         disable adaptive release\n"
        "  --soft-clip\n"
        "  --agc                 automatic gain compensation\n"
        "  --bypass-max          maximizer bypassed\n"
        "EQ options (repeat up to 8 times):\n"
        "  --eq <type>:<freq>:<gain>:<q>[:<slope dB/oct>]\n"
        "       type = bell|lowshelf|highshelf|highpass|lowpass|notch\n"
        "Output options:\n"
        "  --bits <32f|24|16>    output format (default 32f; 24/16 are TPDF dithered)\n"
        "  --report <file.json>  report path (default <output>.report.json)\n"
        "  --block <n>           host block size to simulate (default 512)\n");
}

struct GRCollector : EngineTap
{
    std::vector<std::pair<float, int>> points;
    int lastN = 0;
    void pushPreEQ (const float* const*, int, int n) noexcept override { lastN = n; }
    void pushGainReduction (float peakDb, float) noexcept override { points.emplace_back (peakDb, lastN); }
};

struct Measurement
{
    double integrated = -100, maxShortTerm = -100, maxMomentary = -100;
    double samplePeakDb = -200, truePeakDb = -200;
};

Measurement measure (const wav::Audio& a)
{
    Measurement m;
    LoudnessMeter lm;
    TruePeakMeter tp;
    lm.prepare (a.sampleRate);
    tp.prepare (a.sampleRate);
    const int n = a.numSamples();
    const int chs = std::min (2, a.numChannels());
    float sp = 0.0f;
    const int step = (int) std::lround (a.sampleRate * 0.1);
    for (int s = 0; s < n; s += step)
    {
        const int len = std::min (step, n - s);
        const float* ptrs[2] { a.channels[0].data() + s, chs > 1 ? a.channels[1].data() + s : nullptr };
        lm.process (ptrs, chs, len);
        tp.process (ptrs, chs, len);
        m.maxShortTerm = std::max (m.maxShortTerm, (double) lm.getShortTerm());
        m.maxMomentary = std::max (m.maxMomentary, (double) lm.getMomentary());
        for (int c = 0; c < chs; ++c)
            for (int i = 0; i < len; ++i)
                sp = std::max (sp, std::abs (ptrs[c][i]));
    }
    m.integrated = lm.getIntegrated();
    m.samplePeakDb = sp > 0.0f ? 20.0 * std::log10 (sp) : -200.0;
    m.truePeakDb = tp.getMaxPeak() > 0.0f ? 20.0 * std::log10 (tp.getMaxPeak()) : -200.0;
    return m;
}

bool parseEq (const std::string& spec, EQBandParams& b)
{
    std::vector<std::string> parts;
    std::stringstream ss (spec);
    std::string item;
    while (std::getline (ss, item, ':'))
        parts.push_back (item);
    if (parts.size() < 4)
        return false;
    static const std::map<std::string, FilterType> types {
        { "bell", FilterType::Bell }, { "lowshelf", FilterType::LowShelf }, { "highshelf", FilterType::HighShelf },
        { "highpass", FilterType::HighPass }, { "lowpass", FilterType::LowPass }, { "notch", FilterType::Notch } };
    auto it = types.find (parts[0]);
    if (it == types.end())
        return false;
    b.enabled = true;
    b.type = it->second;
    b.frequency = std::stof (parts[1]);
    b.gainDb = std::stof (parts[2]);
    b.q = std::stof (parts[3]);
    b.slopeIndex = 1;
    if (parts.size() > 4)
    {
        const int s = std::stoi (parts[4]);
        for (size_t k = 0; k < kSlopeChoicesDbPerOct.size(); ++k)
            if (kSlopeChoicesDbPerOct[k] == s)
                b.slopeIndex = (int) k;
    }
    return true;
}

int benchmark (double seconds)
{
    const double fs = 48000.0;
    const int n = (int) (fs * seconds);
    std::vector<std::vector<float>> buf (2, std::vector<float> ((size_t) n));
    uint32_t seed = 1;
    for (int i = 0; i < n; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const float noise = ((float) (seed >> 8) / 16777216.0f - 0.5f) * 0.2f;
        const float v = 0.7f * std::sin (6.2831853f * 60.0f * i / (float) fs) * std::exp (-std::fmod (i / (float) fs, 0.5f) * 8.0f) + noise;
        buf[0][(size_t) i] = v;
        buf[1][(size_t) i] = v * 0.9f;
    }
    std::printf ("CPU benchmark: %.0f s of stereo audio at 48 kHz, 8 EQ bands active, threshold -8 dB\n\n", seconds);
    std::printf ("%-12s %-6s %-11s %-12s %s\n", "mode", "OS", "true peak", "realtime x", "% of one core");
    for (int mode = 0; mode < kNumLimiterModes; ++mode)
        for (int os = 0; os < 5; ++os)
        {
            if (mode != 1 && os != 2)
                continue; // all factors for Modern, default 4x for the other modes
            EngineParams p;
            for (int b = 0; b < kNumEQBands; ++b)
            {
                auto& band = p.eq.bands[(size_t) b];
                band.enabled = true;
                band.frequency = 50.0f * std::pow (2.2f, (float) b);
                band.gainDb = 1.5f;
            }
            p.max.mode = (LimiterMode) mode;
            p.max.oversamplingIndex = os;
            p.max.thresholdDb = -8.0f;
            MasteringEngine e;
            e.prepare (fs, 2);
            e.setParameters (p);
            auto work = buf;
            const auto t0 = std::chrono::steady_clock::now();
            for (int s = 0; s < n; s += 512)
            {
                float* ptrs[2] { work[0].data() + s, work[1].data() + s };
                e.process (ptrs, 2, std::min (512, n - s));
            }
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("%-12s %-6s %-11s %-12.1f %.2f\n", getModeName ((LimiterMode) mode), (std::to_string (1 << os) + "x").c_str(), "on",
                         seconds / secs, 100.0 * secs / seconds);
        }
    return 0;
}

std::string jsonNum (double v) { char b[64]; std::snprintf (b, sizeof (b), "%.3f", v); return b; }
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2 || std::strcmp (argv[1], "--help") == 0 || std::strcmp (argv[1], "-h") == 0)
    {
        usage();
        return argc < 2 ? 1 : 0;
    }
    if (std::strcmp (argv[1], "--benchmark") == 0)
        return benchmark (argc > 2 ? std::atof (argv[2]) : 20.0);
    if (argc < 3)
    {
        usage();
        return 1;
    }

    const std::string inPath = argv[1], outPath = argv[2];
    std::string reportPath = outPath + ".report.json";
    EngineParams p;
    wav::Format format = wav::Format::Float32;
    int block = 512, eqCount = 0;
    std::map<std::string, std::string> settings;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string
        {
            if (i + 1 >= argc) { std::fprintf (stderr, "missing value for %s\n", a.c_str()); std::exit (1); }
            settings[a] = argv[i + 1];
            return argv[++i];
        };
        if (a == "--mode")
        {
            const auto m = next();
            static const std::map<std::string, LimiterMode> modes { { "transparent", LimiterMode::Transparent }, { "modern", LimiterMode::Modern },
                { "aggressive", LimiterMode::Aggressive }, { "punch", LimiterMode::Punch }, { "smooth", LimiterMode::Smooth } };
            if (! modes.count (m)) { std::fprintf (stderr, "unknown mode %s\n", m.c_str()); return 1; }
            p.max.mode = modes.at (m);
        }
        else if (a == "--threshold") p.max.thresholdDb = std::stof (next());
        else if (a == "--ceiling") p.max.ceilingDb = std::stof (next());
        else if (a == "--input-gain") p.max.inputGainDb = std::stof (next());
        else if (a == "--release") p.max.releaseMs = std::stof (next());
        else if (a == "--character") p.max.character = std::stof (next());
        else if (a == "--auto-release") { p.max.autoRelease = true; settings[a] = "on"; }
        else if (a == "--lookahead") p.max.lookaheadMs = std::stof (next());
        else if (a == "--link") p.max.stereoLink = std::stof (next()) * 0.01f;
        else if (a == "--link-mode") p.max.linkMode = next() == "split" ? StereoLinkMode::Split : StereoLinkMode::Standard;
        else if (a == "--transient") p.max.transientEmphasis = std::stof (next()) * 0.01f;
        else if (a == "--os")
        {
            const int f = std::stoi (next());
            int idx = 0;
            while ((1 << idx) < f && idx < 4) ++idx;
            p.max.oversamplingIndex = idx;
        }
        else if (a == "--no-true-peak") { p.max.truePeak = false; settings[a] = "on"; }
        else if (a == "--no-adaptive") { p.max.adaptiveRelease = false; settings[a] = "on"; }
        else if (a == "--soft-clip") { p.max.softClip = true; settings[a] = "on"; }
        else if (a == "--agc") { p.max.autoGain = true; settings[a] = "on"; }
        else if (a == "--bypass-max") { p.max.bypass = true; settings[a] = "on"; }
        else if (a == "--eq")
        {
            const auto spec = next();
            settings["--eq" + std::to_string (eqCount)] = spec;
            if (eqCount >= kNumEQBands || ! parseEq (spec, p.eq.bands[(size_t) eqCount]))
            {
                std::fprintf (stderr, "invalid --eq '%s'\n", spec.c_str());
                return 1;
            }
            ++eqCount;
        }
        else if (a == "--bits")
        {
            const auto b = next();
            format = b == "16" ? wav::Format::Pcm16 : (b == "24" ? wav::Format::Pcm24 : wav::Format::Float32);
        }
        else if (a == "--report") reportPath = next();
        else if (a == "--block") block = std::max (1, std::stoi (next()));
        else { std::fprintf (stderr, "unknown option %s\n", a.c_str()); usage(); return 1; }
    }

    wav::Audio in;
    std::string error;
    if (! wav::read (inPath, in, error))
    {
        std::fprintf (stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (in.numChannels() > 2)
    {
        std::fprintf (stderr, "error: only mono and stereo files are supported\n");
        return 1;
    }

    // Process with latency compensation: pad with `latency` samples of silence at the end
    // and drop the first `latency` samples, so the output is time-aligned with the input.
    MasteringEngine engine;
    GRCollector gr;
    engine.prepare (in.sampleRate, in.numChannels());
    engine.setTap (&gr);
    engine.setParameters (p);
    const int latency = engine.getLatency();
    const int n = in.numSamples();
    const int chs = in.numChannels();

    std::vector<std::vector<float>> work ((size_t) chs, std::vector<float> ((size_t) (n + latency), 0.0f));
    for (int c = 0; c < chs; ++c)
        std::copy (in.channels[(size_t) c].begin(), in.channels[(size_t) c].end(), work[(size_t) c].begin());

    const auto t0 = std::chrono::steady_clock::now();
    for (int s = 0; s < n + latency; s += block)
    {
        float* ptrs[2] { work[0].data() + s, chs > 1 ? work[1].data() + s : nullptr };
        engine.process (ptrs, chs, std::min (block, n + latency - s));
    }
    const double procSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    wav::Audio out;
    out.sampleRate = in.sampleRate;
    out.channels.resize ((size_t) chs);
    for (int c = 0; c < chs; ++c)
        out.channels[(size_t) c].assign (work[(size_t) c].begin() + latency, work[(size_t) c].end());

    if (! wav::write (outPath, out, format, error))
    {
        std::fprintf (stderr, "error: %s\n", error.c_str());
        return 1;
    }

    // Measurements on the files as written (re-read the output so quantisation is included)
    wav::Audio written;
    wav::read (outPath, written, error);
    const auto mi = measure (in), mo = measure (written);

    double grMax = 0.0, grSum = 0.0, grOver1 = 0.0, total = 0.0;
    size_t skip = (size_t) (latency / std::max (1, std::min (block, MasteringEngine::kChunkSize)));
    for (size_t k = skip; k < gr.points.size(); ++k)
    {
        const auto& pt = gr.points[k];
        grMax = std::max (grMax, (double) pt.first);
        grSum += (double) pt.first * pt.second;
        if (pt.first > 1.0f) grOver1 += pt.second;
        total += pt.second;
    }
    const double grMean = total > 0 ? grSum / total : 0.0;
    const double grPct = total > 0 ? 100.0 * grOver1 / total : 0.0;
    const double duration = n / in.sampleRate;

    std::printf ("SKJALD offline processing report\n");
    std::printf ("  input      %s (%d ch, %.0f Hz, %d-bit %s, %.2f s)\n", inPath.c_str(), chs, in.sampleRate, in.sourceBits, in.sourceFloat ? "float" : "PCM", duration);
    std::printf ("  output     %s\n", outPath.c_str());
    std::printf ("  latency    %d samples (%.2f ms, compensated in the output file)\n\n", latency, 1000.0 * latency / in.sampleRate);
    std::printf ("                      input        output\n");
    std::printf ("  integrated      %7.2f LUFS  %7.2f LUFS\n", mi.integrated, mo.integrated);
    std::printf ("  max short-term  %7.2f LUFS  %7.2f LUFS\n", mi.maxShortTerm, mo.maxShortTerm);
    std::printf ("  max momentary   %7.2f LUFS  %7.2f LUFS\n", mi.maxMomentary, mo.maxMomentary);
    std::printf ("  sample peak     %7.2f dBFS  %7.2f dBFS\n", mi.samplePeakDb, mo.samplePeakDb);
    std::printf ("  true peak       %7.2f dBTP  %7.2f dBTP\n\n", mi.truePeakDb, mo.truePeakDb);
    std::printf ("  gain reduction  max %.2f dB, mean %.2f dB, > 1 dB for %.1f %% of the time\n", grMax, grMean, grPct);
    std::printf ("  processing      %.3f s for %.2f s of audio (%.1fx realtime, %.2f %% of one core)\n", procSeconds, duration,
                 duration / procSeconds, 100.0 * procSeconds / duration);

    std::ofstream r (reportPath);
    r << "{\n";
    r << "  \"input\": \"" << inPath << "\",\n  \"output\": \"" << outPath << "\",\n";
    r << "  \"sampleRate\": " << in.sampleRate << ",\n  \"channels\": " << chs << ",\n  \"durationSeconds\": " << jsonNum (duration) << ",\n";
    r << "  \"latencySamples\": " << latency << ",\n";
    r << "  \"settings\": {";
    bool first = true;
    for (const auto& [k, v] : settings)
    {
        r << (first ? "" : ",") << "\n    \"" << k.substr (2) << "\": \"" << v << "\"";
        first = false;
    }
    r << "\n  },\n";
    auto meas = [&r] (const char* name, const Measurement& m, bool last)
    {
        r << "  \"" << name << "\": { \"integratedLufs\": " << jsonNum (m.integrated) << ", \"maxShortTermLufs\": " << jsonNum (m.maxShortTerm)
          << ", \"maxMomentaryLufs\": " << jsonNum (m.maxMomentary) << ", \"samplePeakDbfs\": " << jsonNum (m.samplePeakDb)
          << ", \"truePeakDbtp\": " << jsonNum (m.truePeakDb) << " }" << (last ? "\n" : ",\n");
    };
    meas ("inputMeasurement", mi, false);
    meas ("outputMeasurement", mo, false);
    r << "  \"gainReduction\": { \"maxDb\": " << jsonNum (grMax) << ", \"meanDb\": " << jsonNum (grMean) << ", \"percentAbove1dB\": " << jsonNum (grPct) << " },\n";
    r << "  \"performance\": { \"processingSeconds\": " << jsonNum (procSeconds) << ", \"realtimeFactor\": " << jsonNum (duration / procSeconds)
      << ", \"percentOfOneCore\": " << jsonNum (100.0 * procSeconds / duration) << " }\n";
    r << "}\n";
    std::printf ("  report      %s\n", reportPath.c_str());
    return 0;
}
