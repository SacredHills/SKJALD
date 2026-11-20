#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace skjald::ui
{
/** SKJALD visual identity: charcoal surfaces, warm amber brand accent, ice-blue analysis
    accent. All sizes are in design units (the editor scales the whole window). */
namespace colours
{
    inline const juce::Colour background { 0xff101216 };
    inline const juce::Colour header { 0xff15181d };
    inline const juce::Colour panel { 0xff181b21 };
    inline const juce::Colour panelRaised { 0xff1e222a };
    inline const juce::Colour outline { 0xff2a2f38 };
    inline const juce::Colour outlineStrong { 0xff3a404b };
    inline const juce::Colour grid { 0xff232830 };
    inline const juce::Colour gridStrong { 0xff2f3540 };
    inline const juce::Colour text { 0xffe4e6ea };
    inline const juce::Colour textDim { 0xff8b919b };
    inline const juce::Colour textFaint { 0xff5c626c };
    inline const juce::Colour accent { 0xfff0a830 };      // brand amber
    inline const juce::Colour accentDim { 0xff8a6420 };
    inline const juce::Colour analysis { 0xff5bc0eb };    // ice blue
    inline const juce::Colour gainReduction { 0xffff8a3d };
    inline const juce::Colour danger { 0xffff4d4d };
    inline const juce::Colour ok { 0xff5fd38d };
    inline const juce::Colour knobBody { 0xff262a32 };

    inline juce::Colour band (int index)
    {
        static const juce::Colour c[] = {
            juce::Colour (0xffe86f6f), juce::Colour (0xfff0a830), juce::Colour (0xffe8d65a), juce::Colour (0xff7fd36b),
            juce::Colour (0xff4fd1b6), juce::Colour (0xff5bc0eb), juce::Colour (0xff8f8cf2), juce::Colour (0xffd77fe0),
        };
        return c[(size_t) juce::jlimit (0, 7, index)];
    }
} // namespace colours

inline juce::Font font (float height, bool bold = false)
{
    auto o = juce::FontOptions (height);
    if (bold)
        o = o.withStyle ("Bold");
    return juce::Font (o);
}

inline juce::String formatDb (float v, int decimals = 1)
{
    if (v <= -99.0f)
        return "-inf";
    return (v > 0.0f ? "+" : "") + juce::String (v, decimals);
}

} // namespace skjald::ui
