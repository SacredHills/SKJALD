#pragma once

#include <string>
#include <vector>

namespace skjald::wav
{
/** Minimal, dependency-free RIFF/WAVE reader/writer.
    Reads PCM 16/24/32-bit integer and IEEE float 32/64-bit (incl. WAVE_FORMAT_EXTENSIBLE).
    Writes 16/24-bit PCM (TPDF dithered) or 32-bit float. */
struct Audio
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> channels; // [channel][sample]
    int sourceBits = 0;
    bool sourceFloat = false;

    int numChannels() const { return (int) channels.size(); }
    int numSamples() const { return channels.empty() ? 0 : (int) channels[0].size(); }
};

bool read (const std::string& path, Audio& out, std::string& error);

enum class Format { Float32, Pcm24, Pcm16 };
bool write (const std::string& path, const Audio& audio, Format format, std::string& error);

} // namespace skjald::wav
