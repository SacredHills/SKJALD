#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace skjald::ui
{
/** SKJALD visual identity: black and white. Near-black surfaces, white accent, grey analysis
    colours (red is kept only for clipping / overload warnings). All sizes are in design units (the editor scales the whole window). */
namespace colours
{
    inline const juce::Colour background { 0xff070707 };
    inline const juce::Colour header { 0xff0c0c0c };
    inline const juce::Colour panel { 0xff111111 };
    inline const juce::Colour panelRaised { 0xff191919 };
    inline const juce::Colour outline { 0xff2a2a2a };
    inline const juce::Colour outlineStrong { 0xff444444 };
    inline const juce::Colour grid { 0xff1c1c1c };
    inline const juce::Colour gridStrong { 0xff2c2c2c };
    inline const juce::Colour text { 0xfff0f0f0 };
    inline const juce::Colour textDim { 0xff9a9a9a };
    inline const juce::Colour textFaint { 0xff5e5e5e };
    inline const juce::Colour accent { 0xffffffff };      // white
    inline const juce::Colour accentDim { 0xff8c8c8c };
    inline const juce::Colour analysis { 0xffbdbdbd };    // light grey
    inline const juce::Colour gainReduction { 0xffe2e2e2 };
    inline const juce::Colour danger { 0xffff4d4d };
    inline const juce::Colour ok { 0xffd4d4d4 };
    inline const juce::Colour knobBody { 0xff242424 };

    inline juce::Colour band (int index)
    {
        static const juce::Colour c[] = {
            juce::Colour (0xffffffff), juce::Colour (0xffe0e0e0), juce::Colour (0xffc4c4c4), juce::Colour (0xffa8a8a8),
            juce::Colour (0xff8e8e8e), juce::Colour (0xffd2d2d2), juce::Colour (0xffb6b6b6), juce::Colour (0xff9c9c9c),
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
