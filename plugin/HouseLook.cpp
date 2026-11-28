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

juce::Font HouseLook::uiFont (float size, bool bold)
{
    return juce::Font (juce::FontOptions (uiFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
}

void HouseLook::drawFace (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool highlighted, bool down, bool enabled,
                          bool cl, bool cr) const
{
    const bool dark = isDark();
    const auto mark = pal.accentMark();
    const float radius = juce::jmin (7.0f, r.getHeight() * 0.3f);
    r = r.reduced (0.5f, 0.5f).withTrimmedBottom (1.0f); // room for the shadow
    juce::Path face;
    face.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, ! cl, ! cr, ! cl, ! cr);

    const float alpha = enabled ? 1.0f : 0.45f;
    // drop shadow
    if (! down)
    {
        g.setColour (juce::Colours::black.withAlpha ((dark ? 0.45f : 0.12f) * alpha));
        g.fillPath (face, juce::AffineTransform::translation (0.0f, 1.0f));
    }
    // body
    juce::Colour base = dark ? pal.soft.brighter (0.06f) : juce::Colour (0xfffcfbf8);
    if (on) base = dark ? base.interpolatedWith (mark, 0.20f) : base.interpolatedWith (mark, 0.16f);
    if (highlighted && enabled) base = dark ? base.brighter (0.10f) : base.darker (0.03f);
    auto top = base.brighter (dark ? 0.10f : 0.02f), bottom = base.darker (dark ? 0.12f : 0.05f);
    if (down) std::swap (top, bottom);
    g.setGradientFill (juce::ColourGradient (top.withMultipliedAlpha (alpha), 0, r.getY(), bottom.withMultipliedAlpha (alpha), 0, r.getBottom(), false));
    g.fillPath (face);
    // a fine highlight along the top edge
    if (! down)
    {
        g.setColour (juce::Colours::white.withAlpha ((dark ? 0.07f : 0.8f) * alpha));
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + (cl ? 0.0f : radius * 0.6f), r.getRight() - (cr ? 0.0f : radius * 0.6f));
    }
    // border
    auto border = on ? mark.withAlpha (dark ? 0.85f : 0.9f) : (dark ? pal.line.brighter (0.05f) : pal.line.darker (0.05f));
    g.setColour (border.withMultipliedAlpha (alpha));
    g.strokePath (face, juce::PathStrokeType (1.0f));
}

void HouseLook::drawWell (juce::Graphics& g, juce::Rectangle<float> r) const
{
    const bool dark = isDark();
    r = r.reduced (0.5f, 0.5f).withTrimmedBottom (1.0f);
    const float radius = juce::jmin (7.0f, r.getHeight() * 0.3f);
    g.setColour (dark ? pal.page.darker (0.15f) : juce::Colour (0xffe9e7e0));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (dark ? 0.35f : 0.08f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + radius * 0.6f, r.getRight() - radius * 0.6f);
    g.setColour (dark ? pal.line : pal.line.darker (0.05f));
    g.drawRoundedRectangle (r, radius, 1.0f);
}

juce::Colour HouseLook::faceInk (bool on, bool toggle, bool enabled) const
{
    juce::Colour c = pal.ink;
    if (toggle) c = on ? pal.accentMark() : pal.ink.interpolatedWith (pal.muted, 0.55f);
    if (on && ! isDark()) c = pal.accentMark().darker (0.25f);
    return enabled ? c : c.withMultipliedAlpha (0.4f);
}

void HouseLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    drawFace (g, b.getLocalBounds().toFloat(), b.getToggleState(), highlighted, down, b.isEnabled(),
              b.isConnectedOnLeft(), b.isConnectedOnRight());
}

void HouseLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool toggle = b.getClickingTogglesState() || b.getRadioGroupId() != 0;
    g.setColour (faceInk (b.getToggleState(), toggle, b.isEnabled()));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0).translated (0, down ? 0 : -1), juce::Justification::centred, 1);
}

void HouseLook::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (isDark() ? pal.panel.brighter (0.1f) : juce::Colour (0xfffcfbf8));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (pal.line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (pal.ink);
    g.setFont (uiFont (12.5f, false));
    g.drawFittedText (text, r.reduced (10, 4).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> HouseLook::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = uiFont (12.5f, false);
    const int w = juce::jmin (420, juce::roundToInt (juce::GlyphArrangement::getStringWidth (f, tipText)) + 24);
    const int lines = juce::roundToInt (juce::GlyphArrangement::getStringWidth (f, tipText) + 24) / 420 + 1;
    const int h = 10 + lines * 17;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

juce::Font HouseLook::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (uiFontName(), 12.5f, juce::Font::bold));
}

void HouseLook::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    juce::ignoreUnused (down);
    drawFace (g, { 0, 0, (float) width, (float) height }, false, box.isMouseOver (true), box.isPopupActive(), box.isEnabled());
    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f - 1.0f;
    arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (pal.muted);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font HouseLook::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (uiFontName(), 12.5f, juce::Font::bold)); }

void HouseLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font HouseLook::getPopupMenuFont() { return juce::Font (juce::FontOptions (uiFontName(), 14.0f, juce::Font::plain)); }

juce::Font HouseLook::getLabelFont (juce::Label& l) { return l.getFont().withTypefaceStyle (l.getFont().getTypefaceStyle()); }

void HouseLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                  juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        drawWell (g, r);
        if (s.isEnabled())
        {
            auto in = r.reduced (2.0f, 2.0f).withTrimmedBottom (1.0f);
            g.setColour (pal.accentMark().withAlpha (0.16f));
            g.fillRoundedRectangle (in.withWidth (juce::jmax (0.0f, pos - in.getX())), 5.0f);
        }
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
