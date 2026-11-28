// The look shared with Graham's other apps (after vii°/V): system font, warm
// dark greys, 1px line borders, rounded controls, and toggles that turn from
// muted text to the accent colour when on.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class HouseLook : public juce::LookAndFeel_V4
{
public:
    struct Palette
    {
        juce::Colour page, panel, soft, line, ink, muted, accent;
        static Palette dark (juce::Colour accent);
        static Palette light (juce::Colour accent);
        juce::Colour accentMark() const; // the accent, readable on `panel`
    };

    HouseLook();
    void setPalette (const Palette& p);
    const Palette& palette() const { return pal; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static juce::String uiFontName();
    static juce::Font uiFont (float size, bool bold = true);

    bool isDark() const { return pal.page.getPerceivedBrightness() < 0.5f; }
    // A raised, rounded control face: soft gradient, hairline border, a little
    // shadow; lit with the accent when on; pressed sinks in. cl/cr = joined to a
    // neighbour on the left/right (segmented groups).
    void drawFace (juce::Graphics&, juce::Rectangle<float>, bool on, bool highlighted, bool down, bool enabled,
                   bool cl = false, bool cr = false) const;
    // A sunken field (value displays).
    void drawWell (juce::Graphics&, juce::Rectangle<float>) const;
    // The colour for text or an icon on a face.
    juce::Colour faceInk (bool on, bool toggle, bool enabled) const;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;

private:
    Palette pal;
};
