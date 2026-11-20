#include "WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>

namespace skjald::wav
{
namespace
{
    uint16_t le16 (const unsigned char* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
    uint32_t le32 (const unsigned char* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }

    void put16 (std::vector<unsigned char>& v, uint16_t x) { v.push_back ((unsigned char) (x & 0xff)); v.push_back ((unsigned char) (x >> 8)); }
    void put32 (std::vector<unsigned char>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back ((unsigned char) ((x >> (8 * i)) & 0xff)); }
    void putTag (std::vector<unsigned char>& v, const char* t) { v.insert (v.end(), t, t + 4); }
} // namespace

bool read (const std::string& path, Audio& out, std::string& error)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
    {
        error = "cannot open " + path;
        return false;
    }
    std::vector<unsigned char> data ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (data.size() < 12 || std::memcmp (data.data(), "RIFF", 4) != 0 || std::memcmp (data.data() + 8, "WAVE", 4) != 0)
    {
        error = "not a RIFF/WAVE file";
        return false;
    }

    int format = 0, channels = 0, bits = 0, blockAlign = 0;
    uint32_t rate = 0;
    const unsigned char* pcm = nullptr;
    size_t pcmBytes = 0;

    size_t pos = 12;
    while (pos + 8 <= data.size())
    {
        const unsigned char* chunk = data.data() + pos;
        const uint32_t size = le32 (chunk + 4);
        const size_t body = pos + 8;
        if (body + size > data.size() && std::memcmp (chunk, "data", 4) != 0)
            break;
        if (std::memcmp (chunk, "fmt ", 4) == 0 && size >= 16)
        {
            format = le16 (chunk + 8);
            channels = le16 (chunk + 10);
            rate = le32 (chunk + 12);
            blockAlign = le16 (chunk + 20);
            bits = le16 (chunk + 22);
            if (format == 0xFFFE && size >= 40)
                format = le16 (chunk + 8 + 24); // sub-format GUID starts with the format tag
        }
        else if (std::memcmp (chunk, "data", 4) == 0)
        {
            pcm = data.data() + body;
            pcmBytes = std::min ((size_t) size, data.size() - body);
        }
        pos = body + size + (size & 1);
    }

    if (pcm == nullptr || channels <= 0 || rate == 0 || blockAlign <= 0)
    {
        error = "missing fmt or data chunk";
        return false;
    }
    const bool isFloat = format == 3;
    if (! (format == 1 || isFloat) || (isFloat && bits != 32 && bits != 64) || (! isFloat && bits != 16 && bits != 24 && bits != 32))
    {
        error = "unsupported sample format (format " + std::to_string (format) + ", " + std::to_string (bits) + " bits)";
        return false;
    }

    const size_t frames = pcmBytes / (size_t) blockAlign;
    out.sampleRate = rate;
    out.sourceBits = bits;
    out.sourceFloat = isFloat;
    out.channels.assign ((size_t) channels, std::vector<float> (frames, 0.0f));
    const int bytes = bits / 8;
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c)
        {
            const unsigned char* s = pcm + i * (size_t) blockAlign + (size_t) (c * bytes);
            float v = 0.0f;
            if (isFloat && bits == 32) { float x; std::memcpy (&x, s, 4); v = x; }
            else if (isFloat) { double x; std::memcpy (&x, s, 8); v = (float) x; }
            else if (bits == 16) v = (float) (int16_t) le16 (s) / 32768.0f;
            else if (bits == 24) { int32_t x = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8; v = (float) x / 8388608.0f; }
            else v = (float) ((double) (int32_t) le32 (s) / 2147483648.0);
            out.channels[(size_t) c][i] = v;
        }
    return true;
}

bool write (const std::string& path, const Audio& audio, Format format, std::string& error)
{
    const int channels = audio.numChannels();
    const int frames = audio.numSamples();
    const int bits = format == Format::Float32 ? 32 : (format == Format::Pcm24 ? 24 : 16);
    const int bytes = bits / 8;
    const uint32_t dataBytes = (uint32_t) frames * (uint32_t) channels * (uint32_t) bytes;

    std::vector<unsigned char> v;
    v.reserve (dataBytes + 64);
    putTag (v, "RIFF");
    put32 (v, 36 + dataBytes + (dataBytes & 1));
    putTag (v, "WAVE");
    putTag (v, "fmt ");
    put32 (v, 16);
    put16 (v, format == Format::Float32 ? 3 : 1);
    put16 (v, (uint16_t) channels);
    put32 (v, (uint32_t) std::lround (audio.sampleRate));
    put32 (v, (uint32_t) std::lround (audio.sampleRate) * (uint32_t) (channels * bytes));
    put16 (v, (uint16_t) (channels * bytes));
    put16 (v, (uint16_t) bits);
    putTag (v, "data");
    put32 (v, dataBytes);

    // TPDF dither (+/- 1 LSB) for integer formats
    std::mt19937 rng (12345u);
    std::uniform_real_distribution<double> uni (-0.5, 0.5);
    const double scale = format == Format::Pcm24 ? 8388608.0 : 32768.0;
    const double maxv = scale - 1.0;

    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c)
        {
            const float x = audio.channels[(size_t) c][(size_t) i];
            if (format == Format::Float32)
            {
                unsigned char b[4];
                std::memcpy (b, &x, 4);
                v.insert (v.end(), b, b + 4);
                continue;
            }
            const double d = std::clamp (std::round (x * scale + uni (rng) + uni (rng)), -scale, maxv);
            const int32_t q = (int32_t) d;
            if (format == Format::Pcm16)
                put16 (v, (uint16_t) (int16_t) q);
            else
            {
                v.push_back ((unsigned char) (q & 0xff));
                v.push_back ((unsigned char) ((q >> 8) & 0xff));
                v.push_back ((unsigned char) ((q >> 16) & 0xff));
            }
        }
    if (dataBytes & 1)
        v.push_back (0);

    std::ofstream f (path, std::ios::binary);
    if (! f)
    {
        error = "cannot write " + path;
        return false;
    }
    f.write ((const char*) v.data(), (std::streamsize) v.size());
    return (bool) f;
}

} // namespace skjald::wav
