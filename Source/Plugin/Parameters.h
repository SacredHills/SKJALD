#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/MasteringEngine.h"

#include <array>

namespace skjald::params
{
// ---- Global ---------------------------------------------------------------------------
inline constexpr const char* globalBypass = "globalBypass";
inline constexpr const char* eqBypass = "eqBypass";
inline constexpr const char* maxBypass = "maxBypass";

// ---- Maximizer -----------------------------------------------------------------------
inline constexpr const char* inputGain = "inGain";
inline constexpr const char* threshold = "threshold";
inline constexpr const char* ceiling = "ceiling";
inline constexpr const char* release = "release";
inline constexpr const char* autoRelease = "autoRelease";
inline constexpr const char* character = "character";
inline constexpr const char* lookahead = "lookahead";
inline constexpr const char* stereoLink = "stereoLink";
inline constexpr const char* transient = "transient";
inline constexpr const char* mode = "mode";
inline constexpr const char* truePeak = "truePeak";
inline constexpr const char* oversampling = "oversampling";
inline constexpr const char* adaptiveRelease = "adaptiveRelease";
inline constexpr const char* softClip = "softClip";
inline constexpr const char* linkMode = "linkMode";
inline constexpr const char* autoGain = "autoGain";
inline constexpr const char* delta = "delta";

// ---- EQ bands (1-based in IDs) ---------------------------------------------------------
enum class BandParam { On, Type, Freq, Gain, Q, Slope };
juce::String bandID (int bandIndex0, BandParam which);

struct BandDefaults
{
    FilterType type;
    float frequency;
    float gainDb;
    float q;
    int slopeIndex;
};
const std::array<BandDefaults, kNumEQBands>& bandDefaults();

const juce::StringArray& filterTypeNames();
const juce::StringArray& slopeNames();
const juce::StringArray& modeNames();
const juce::StringArray& oversamplingNames();
const juce::StringArray& linkModeNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** IDs of all parameters belonging to a section (used by presets and A/B). */
juce::StringArray eqParameterIDs();
juce::StringArray maximizerParameterIDs();

/** Lock-free snapshot of every parameter into the engine parameter struct. */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

private:
    struct BandPtrs
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* slope = nullptr;
    };
    std::array<BandPtrs, kNumEQBands> bands {};
    std::atomic<float>* pGlobalBypass = nullptr;
    std::atomic<float>* pEqBypass = nullptr;
    std::atomic<float>* pMaxBypass = nullptr;
    std::atomic<float>* pInputGain = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pAutoRelease = nullptr;
    std::atomic<float>* pCharacter = nullptr;
    std::atomic<float>* pLookahead = nullptr;
    std::atomic<float>* pStereoLink = nullptr;
    std::atomic<float>* pTransient = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pTruePeak = nullptr;
    std::atomic<float>* pOversampling = nullptr;
    std::atomic<float>* pAdaptive = nullptr;
    std::atomic<float>* pSoftClip = nullptr;
    std::atomic<float>* pLinkMode = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pDelta = nullptr;
};

/** Reads the EQ section from the parameter state (GUI thread helper for drawing curves). */
EQParams readEQ (juce::AudioProcessorValueTreeState& state);

} // namespace skjald::params
