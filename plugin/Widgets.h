// Buttons with drawn icons or music glyphs, in the house look.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

enum class Icon
{
    None,
    Play, Stop, Rewind, Undo, Redo, Plus, Minus, Duplicate, Trash, Loop, Sliders, Keyboard,
    Export, Drag, Sun, Moon, Speaker, Pencil, Arrow, Insert, StaffUp, StaffDown, Rename,
    Copy, Cut, Paste, Erase, Bars, Menu
};

// The icon drawn into a box (strokes and fills in the current colour).
void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> box, float stroke = 1.5f);

class IconButton : public juce::Button
{
public:
    explicit IconButton (Icon icon = Icon::None, const juce::String& text = {});

    void setIcon (Icon i) { icon = i; repaint(); }
    // A music glyph (Bravura) instead of a drawn icon; size is the em in px.
    void setGlyph (const juce::String& glyphs, float size, float yOffset = 0.0f);
    void setText (const juce::String& t) { setButtonText (t); repaint(); }
    // Draws this swatch dot on the right (used by the colour button).
    void setSwatch (juce::Colour c) { swatch = c; repaint(); }
    // Text in the glyph font only (e.g. dynamics), no icon.
    bool toggleLook = false; // muted when off, accent when on, even if it doesn't toggle itself

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    int idealWidth() const;

private:
    Icon icon;
    juce::String glyph;
    float glyphSize = 0, glyphY = 0;
    juce::Colour swatch = juce::Colours::transparentBlack;
};
