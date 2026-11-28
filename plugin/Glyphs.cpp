#include "Glyphs.h"

#include "StaffPaperFonts.h"

juce::Typeface::Ptr MusicFont::typeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (StaffPaperFonts::Bravura_otf,
                                                                             (size_t) StaffPaperFonts::Bravura_otfSize);
    return tf;
}

juce::Font MusicFont::forStaffSpace (float space)
{
    // SMuFL: the em is four staff spaces. JUCE's point height is the em size.
    return juce::Font (juce::FontOptions (typeface()).withPointHeight (space * 4.0f));
}
