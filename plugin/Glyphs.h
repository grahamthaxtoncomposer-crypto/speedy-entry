// SMuFL glyph code points (Bravura font, SIL Open Font Licence) and the font loader.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace Glyph
{
// clefs
constexpr juce::juce_wchar gClef = 0xE050, gClef8vb = 0xE052, gClef8va = 0xE053;
constexpr juce::juce_wchar cClef = 0xE05C;
constexpr juce::juce_wchar fClef = 0xE062, fClef8vb = 0xE064, fClef8va = 0xE065;
constexpr juce::juce_wchar percussionClef = 0xE069;
constexpr juce::juce_wchar gClefChange = 0xE07A, cClefChange = 0xE07B, fClefChange = 0xE07C;
// noteheads
constexpr juce::juce_wchar noteheadDoubleWhole = 0xE0A0, noteheadWhole = 0xE0A2, noteheadHalf = 0xE0A3, noteheadBlack = 0xE0A4;
// flags (up, down) for 8th, 16th, 32nd, 64th
constexpr juce::juce_wchar flagUp[4] = { 0xE240, 0xE242, 0xE244, 0xE246 };
constexpr juce::juce_wchar flagDown[4] = { 0xE241, 0xE243, 0xE245, 0xE247 };
// rests: double whole, whole, half, quarter, 8th, 16th, 32nd, 64th
constexpr juce::juce_wchar restDoubleWhole = 0xE4E2, restWhole = 0xE4E3, restHalf = 0xE4E4, restQuarter = 0xE4E5;
constexpr juce::juce_wchar rest8th = 0xE4E6, rest16th = 0xE4E7, rest32nd = 0xE4E8, rest64th = 0xE4E9;
// accidentals
constexpr juce::juce_wchar flat = 0xE260, natural = 0xE261, sharp = 0xE262, doubleSharp = 0xE263, doubleFlat = 0xE264;
constexpr juce::juce_wchar parenLeft = 0xE26A, parenRight = 0xE26B;
constexpr juce::juce_wchar augmentationDot = 0xE1E7;
// articulations (above, below)
constexpr juce::juce_wchar accentAbove = 0xE4A0, accentBelow = 0xE4A1;
constexpr juce::juce_wchar staccatoAbove = 0xE4A2, staccatoBelow = 0xE4A3;
constexpr juce::juce_wchar tenutoAbove = 0xE4A4, tenutoBelow = 0xE4A5;
// time signature digits 0..9
constexpr juce::juce_wchar timeSig0 = 0xE080;
constexpr juce::juce_wchar tuplet0 = 0xE880;
constexpr juce::juce_wchar brace = 0xE000;
} // namespace Glyph

class MusicFont
{
public:
    static juce::Typeface::Ptr typeface();
    // A font in which one em equals four staff spaces.
    static juce::Font forStaffSpace (float space);
};
