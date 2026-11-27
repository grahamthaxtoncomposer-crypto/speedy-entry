#include "HouseLook.h"

HouseLook::Palette HouseLook::Palette::dark (juce::Colour accent)
{
    return { juce::Colour (0xff171916), juce::Colour (0xff20231f), juce::Colour (0xff2a2d28), juce::Colour (0xff41443d),
             juce::Colour (0xfff2f0e9), juce::Colour (0xffa4a69c), accent };
}

HouseLook::Palette HouseLook::Palette::light (juce::Colour accent)
{
    return { juce::Colour (0xfff6f5f0), juce::Colour (0xffecebe4), juce::Colour (0xffe2e0d8), juce::Colour (0xffcfcdc4),
             juce::Colour (0xff171916), juce::Colour (0xff6b6d64), accent };
}

juce::Colour HouseLook::Palette::accentMark() const
{
    juce::Colour c = accent;
    if (panel.getPerceivedBrightness() > 0.5f)
        for (int i = 0; i < 6 && c.getPerceivedBrightness() > 0.55f; ++i) c = c.darker (0.35f);
    else
        for (int i = 0; i < 6 && c.getPerceivedBrightness() < 0.45f; ++i) c = c.brighter (0.35f);
    return c;
}

juce::String HouseLook::uiFontName()
{
    // vii°/V uses the Apple system font. Plug-ins can only ask for installed
    // families by name, so take SF Pro when it is installed, else the closest.
    static const juce::String name = [] {
        const auto names = juce::Font::findAllTypefaceNames();
        for (auto want : { "SF Pro Text", "SF Pro", "SF Pro Display", "Helvetica Neue", "Inter" })
            if (names.contains (want)) return juce::String (want);
        return juce::Font::getDefaultSansSerifFontName();
    }();
    return name;
}

HouseLook::HouseLook()
{
    setDefaultSansSerifTypefaceName (uiFontName());
    setPalette (Palette::dark (juce::Colour (0xffccff66)));
}

void HouseLook::setPalette (const Palette& p)
{
    pal = p;
    const auto mark = p.accentMark();
    setColour (juce::ResizableWindow::backgroundColourId, p.page);
    setColour (juce::TextButton::buttonColourId, p.soft);
    setColour (juce::TextButton::buttonOnColourId, mark);
    setColour (juce::TextButton::textColourOffId, p.ink);
    setColour (juce::TextButton::textColourOnId, mark);
    setColour (juce::Label::textColourId, p.ink);
    setColour (juce::ToggleButton::textColourId, p.ink);
    setColour (juce::ToggleButton::tickColourId, mark);
    setColour (juce::ToggleButton::tickDisabledColourId, p.muted);
    setColour (juce::ComboBox::backgroundColourId, p.soft);
    setColour (juce::ComboBox::textColourId, p.ink);
    setColour (juce::ComboBox::outlineColourId, p.line);
    setColour (juce::ComboBox::arrowColourId, p.muted);
    setColour (juce::PopupMenu::backgroundColourId, p.panel);
    setColour (juce::PopupMenu::textColourId, p.ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, mark.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.ink);
    setColour (juce::PopupMenu::headerTextColourId, p.muted);
    setColour (juce::Slider::textBoxTextColourId, p.ink);
    setColour (juce::Slider::textBoxBackgroundColourId, p.soft);
    setColour (juce::Slider::textBoxOutlineColourId, p.line);
    setColour (juce::Slider::trackColourId, mark.withAlpha (0.35f));
    setColour (juce::Slider::backgroundColourId, p.soft);
    setColour (juce::Slider::thumbColourId, mark);
    setColour (juce::TextEditor::backgroundColourId, p.soft);
    setColour (juce::TextEditor::textColourId, p.ink);
    setColour (juce::TextEditor::outlineColourId, p.line);
    setColour (juce::TextEditor::focusedOutlineColourId, mark);
    setColour (juce::AlertWindow::backgroundColourId, p.panel);
    setColour (juce::AlertWindow::textColourId, p.ink);
    setColour (juce::AlertWindow::outlineColourId, p.line);
    setColour (juce::ScrollBar::thumbColourId, p.line);
    auto scheme = getCurrentColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, p.panel);
    scheme.setUIColour (ColourScheme::widgetBackground, p.soft);
    scheme.setUIColour (ColourScheme::menuBackground, p.panel);
    scheme.setUIColour (ColourScheme::outline, p.line);
    scheme.setUIColour (ColourScheme::defaultText, p.ink);
    scheme.setUIColour (ColourScheme::defaultFill, p.soft);
    scheme.setUIColour (ColourScheme::highlightedText, p.ink);
    scheme.setUIColour (ColourScheme::highlightedFill, mark.withAlpha (0.3f));
    scheme.setUIColour (ColourScheme::menuText, p.ink);
    setColourScheme (scheme);
}

void HouseLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = juce::jmin (10.0f, r.getHeight() * 0.32f);
    const bool on = b.getToggleState();
    const auto mark = pal.accentMark();
    auto fill = pal.soft;
    if (on) fill = pal.soft.interpolatedWith (mark, 0.12f);
    if (highlighted) fill = fill.brighter (pal.page.getPerceivedBrightness() > 0.5f ? 0.0f : 0.06f).darker (pal.page.getPerceivedBrightness() > 0.5f ? 0.04f : 0.0f);
    if (down) fill = fill.interpolatedWith (mark, 0.12f);
    if (! b.isEnabled()) fill = fill.withMultipliedAlpha (0.5f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (on ? mark : pal.line);
    g.drawRoundedRectangle (r, radius, 1.0f);
}

void HouseLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool toggle = b.getClickingTogglesState() || b.getRadioGroupId() != 0;
    juce::Colour c = pal.ink;
    if (toggle) c = b.getToggleState() ? pal.accentMark() : pal.muted;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.45f);
    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

juce::Font HouseLook::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (uiFontName(), 12.5f, juce::Font::bold));
}

void HouseLook::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    const float radius = juce::jmin (10.0f, r.getHeight() * 0.32f);
    g.setColour (pal.soft);
    g.fillRoundedRectangle (r, radius);
    g.setColour (box.hasKeyboardFocus (true) ? pal.accentMark() : pal.line);
    g.drawRoundedRectangle (r, radius, 1.0f);
    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (pal.muted);
    g.fillPath (arrow);
}

juce::Font HouseLook::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (uiFontName(), 12.5f, juce::Font::bold)); }

void HouseLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font HouseLook::getPopupMenuFont() { return juce::Font (juce::FontOptions (uiFontName(), 14.0f, juce::Font::plain)); }

juce::Font HouseLook::getLabelFont (juce::Label& l) { return l.getFont().withTypefaceStyle (l.getFont().getTypefaceStyle()); }

void HouseLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                  juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        const float radius = juce::jmin (10.0f, r.getHeight() * 0.32f);
        g.setColour (pal.soft);
        g.fillRoundedRectangle (r, radius);
        if (s.isEnabled())
        {
            g.setColour (pal.accentMark().withAlpha (0.18f));
            g.fillRoundedRectangle (r.withWidth (juce::jmax (0.0f, pos - (float) x)), radius);
        }
        g.setColour (pal.line);
        g.drawRoundedRectangle (r, radius, 1.0f);
        return;
    }
    // plain horizontal slider: thin track, accent fill, round thumb
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (pal.soft);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (pal.accentMark().withAlpha (0.7f));
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.setColour (pal.accentMark());
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    juce::ignoreUnused (min, max);
}
