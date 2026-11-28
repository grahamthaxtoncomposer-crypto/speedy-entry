#include "Widgets.h"

#include "Glyphs.h"
#include "HouseLook.h"

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> box, float stroke)
{
    // icons are drawn on a 16 x 16 grid
    const float s = std::min (box.getWidth(), box.getHeight()) / 16.0f;
    const float ox = box.getCentreX() - 8.0f * s, oy = box.getCentreY() - 8.0f * s;
    auto P = [&] (float x, float y) { return juce::Point<float> (ox + x * s, oy + y * s); };
    juce::Path p;
    auto line = [&] (float x1, float y1, float x2, float y2) {
        p.startNewSubPath (P (x1, y1));
        p.lineTo (P (x2, y2));
    };
    auto rrect = [&] (float x, float y, float w, float h, float r) { p.addRoundedRectangle (ox + x * s, oy + y * s, w * s, h * s, r * s); };
    const juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    auto strokeIt = [&] { g.strokePath (p, st); };
    auto fillIt = [&] { g.fillPath (p); };
    auto arrowHead = [&] (float x, float y, float dx, float dy) { // open chevron pointing along (dx, dy)
        const float nx = -dy, ny = dx;
        line (x - dx * 3 + nx * 3, y - dy * 3 + ny * 3, x, y);
        p.lineTo (P (x - dx * 3 - nx * 3, y - dy * 3 - ny * 3));
    };

    switch (icon)
    {
        case Icon::None: return;
        case Icon::Play:
            p.addTriangle (P (4.5f, 3), P (4.5f, 13), P (13, 8));
            fillIt();
            return;
        case Icon::Stop:
            rrect (4, 4, 8, 8, 1);
            fillIt();
            return;
        case Icon::Rewind:
            rrect (3, 3.5f, 2, 9, 0.5f);
            p.addTriangle (P (13, 3.5f), P (13, 12.5f), P (5.5f, 8));
            fillIt();
            return;
        case Icon::Undo:
        case Icon::Redo:
        {
            const bool undo = icon == Icon::Undo;
            auto X = [&] (float x) { return undo ? x : 16 - x; };
            p.startNewSubPath (P (X (4), 7));
            p.lineTo (P (X (10), 7));
            p.quadraticTo (P (X (13.5f), 7), P (X (13.5f), 10));
            p.quadraticTo (P (X (13.5f), 13), P (X (10), 13));
            p.lineTo (P (X (7), 13));
            line (X (7), 4, X (4), 7);
            p.lineTo (P (X (7), 10));
            strokeIt();
            return;
        }
        case Icon::Plus: line (8, 3, 8, 13); line (3, 8, 13, 8); strokeIt(); return;
        case Icon::Minus: line (3, 8, 13, 8); strokeIt(); return;
        case Icon::Duplicate:
            rrect (2.5f, 2.5f, 8, 8, 1.5f);
            p.startNewSubPath (P (6, 13.5f));
            p.lineTo (P (12, 13.5f));
            p.quadraticTo (P (13.5f, 13.5f), P (13.5f, 12));
            p.lineTo (P (13.5f, 6));
            strokeIt();
            return;
        case Icon::Trash:
            line (2.5f, 4.5f, 13.5f, 4.5f);
            line (6, 4.5f, 6.5f, 2.5f);
            p.lineTo (P (9.5f, 2.5f));
            p.lineTo (P (10, 4.5f));
            p.startNewSubPath (P (4, 4.5f));
            p.lineTo (P (4.8f, 13.5f));
            p.lineTo (P (11.2f, 13.5f));
            p.lineTo (P (12, 4.5f));
            line (6.8f, 7, 7, 11);
            line (9.2f, 7, 9, 11);
            strokeIt();
            return;
        case Icon::Loop:
            p.startNewSubPath (P (3, 9));
            p.lineTo (P (3, 7));
            p.quadraticTo (P (3, 4.5f), P (5.5f, 4.5f));
            p.lineTo (P (12.5f, 4.5f));
            arrowHead (12.5f, 4.5f, 1, 0);
            p.startNewSubPath (P (13, 7));
            p.lineTo (P (13, 9));
            p.quadraticTo (P (13, 11.5f), P (10.5f, 11.5f));
            p.lineTo (P (3.5f, 11.5f));
            arrowHead (3.5f, 11.5f, -1, 0);
            strokeIt();
            return;
        case Icon::Sliders:
            line (2.5f, 4, 13.5f, 4); line (2.5f, 8, 13.5f, 8); line (2.5f, 12, 13.5f, 12);
            strokeIt();
            p.clear();
            p.addEllipse (P (8.5f, 2).x, P (8.5f, 2).y, 4 * s, 4 * s);
            p.addEllipse (P (3.5f, 6).x, P (3.5f, 6).y, 4 * s, 4 * s);
            p.addEllipse (P (7, 10).x, P (7, 10).y, 4 * s, 4 * s);
            fillIt();
            return;
        case Icon::Keyboard:
            rrect (1.5f, 4, 13, 8.5f, 1.5f);
            strokeIt();
            p.clear();
            for (int row = 0; row < 2; ++row)
                for (int k = 0; k < 5; ++k)
                    p.addRectangle (P (3.4f + k * 2.1f, 5.8f + row * 2.2f).x, P (0, 5.8f + row * 2.2f).y, 1.1f * s, 1.1f * s);
            p.addRectangle (P (5, 10.3f).x, P (5, 10.3f).y, 6 * s, 1.1f * s);
            fillIt();
            return;
        case Icon::Export:
            p.startNewSubPath (P (5, 6.5f));
            p.lineTo (P (3, 6.5f));
            p.lineTo (P (3, 13.5f));
            p.lineTo (P (13, 13.5f));
            p.lineTo (P (13, 6.5f));
            p.lineTo (P (11, 6.5f));
            line (8, 10, 8, 1.8f);
            arrowHead (8, 1.8f, 0, -1);
            strokeIt();
            return;
        case Icon::Drag:
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 2; ++c)
                    p.addEllipse (P (5.2f + c * 4, 3.2f + r * 4).x, P (0, 3.2f + r * 4).y, 1.8f * s, 1.8f * s);
            fillIt();
            return;
        case Icon::Sun:
            p.addEllipse (P (5, 5).x, P (5, 5).y, 6 * s, 6 * s);
            for (int i = 0; i < 8; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::pi / 4.0f;
                line (8 + std::cos (a) * 5.2f, 8 + std::sin (a) * 5.2f, 8 + std::cos (a) * 6.8f, 8 + std::sin (a) * 6.8f);
            }
            strokeIt();
            return;
        case Icon::Moon:
        {
            p.startNewSubPath (P (11.5f, 2.6f));
            p.cubicTo (P (6, 2.5f), P (4.5f, 9), P (8.5f, 12));
            p.cubicTo (P (10.5f, 13.4f), P (13, 13), P (14, 11.5f));
            p.cubicTo (P (10, 12.5f), P (7, 7), P (11.5f, 2.6f));
            p.closeSubPath();
            p.applyTransform (juce::AffineTransform::translation (-1.5f * s, 0));
            fillIt();
            return;
        }
        case Icon::Speaker:
            p.startNewSubPath (P (2.5f, 6));
            p.lineTo (P (5, 6));
            p.lineTo (P (8.5f, 3));
            p.lineTo (P (8.5f, 13));
            p.lineTo (P (5, 10));
            p.lineTo (P (2.5f, 10));
            p.closeSubPath();
            strokeIt();
            p.clear();
            p.addCentredArc (P (8.5f, 8).x, P (8.5f, 8).y, 2.8f * s, 2.8f * s, 0, 0.6f, 2.55f, true);
            p.addCentredArc (P (8.5f, 8).x, P (8.5f, 8).y, 5.3f * s, 5.3f * s, 0, 0.6f, 2.55f, true);
            strokeIt();
            return;
        case Icon::Pencil:
            p.startNewSubPath (P (3, 13));
            p.lineTo (P (3.6f, 10.2f));
            p.lineTo (P (10.8f, 3));
            p.lineTo (P (13, 5.2f));
            p.lineTo (P (5.8f, 12.4f));
            p.closeSubPath();
            line (9.3f, 4.5f, 11.5f, 6.7f);
            strokeIt();
            return;
        case Icon::Arrow:
            p.startNewSubPath (P (4, 2.5f));
            p.lineTo (P (4, 13));
            p.lineTo (P (6.8f, 10.4f));
            p.lineTo (P (8.8f, 14));
            p.lineTo (P (10.4f, 13.2f));
            p.lineTo (P (8.5f, 9.6f));
            p.lineTo (P (12.2f, 9.4f));
            p.closeSubPath();
            strokeIt();
            return;
        case Icon::Insert:
            line (5, 2.5f, 5, 13.5f);
            line (3.5f, 2.5f, 6.5f, 2.5f);
            line (3.5f, 13.5f, 6.5f, 13.5f);
            line (8, 8, 13.5f, 8);
            arrowHead (13.5f, 8, 1, 0);
            strokeIt();
            return;
        case Icon::StaffUp:
        case Icon::StaffDown:
        {
            const bool up = icon == Icon::StaffUp;
            for (int i = 0; i < 3; ++i) line (1.5f, (up ? 3.0f : 10.0f) + i * 2.0f, 9, (up ? 3.0f : 10.0f) + i * 2.0f);
            strokeIt();
            p.clear();
            line (12.5f, up ? 13 : 3, 12.5f, up ? 3.5f : 12.5f);
            arrowHead (12.5f, up ? 3.5f : 12.5f, 0, up ? -1.0f : 1.0f);
            strokeIt();
            p.clear();
            const float hy = up ? 12.0f : 4.0f;
            p.addEllipse (P (3, hy - 1.5f).x, P (3, hy - 1.5f).y, 4 * s, 3 * s);
            fillIt();
            return;
        }
        case Icon::Rename:
            rrect (1.5f, 4.5f, 13, 7, 1.5f);
            line (5, 6.5f, 5, 9.5f);
            strokeIt();
            return;
        case Icon::Copy:
            rrect (2.5f, 2.5f, 8, 8, 1.5f);
            rrect (5.5f, 5.5f, 8, 8, 1.5f);
            strokeIt();
            return;
        case Icon::Cut:
            p.addEllipse (P (2.5f, 10).x, P (2.5f, 10).y, 3.5f * s, 3.5f * s);
            p.addEllipse (P (10, 10).x, P (10, 10).y, 3.5f * s, 3.5f * s);
            line (5.3f, 10.5f, 11.5f, 2.5f);
            line (10.7f, 10.5f, 4.5f, 2.5f);
            strokeIt();
            return;
        case Icon::Paste:
            rrect (3, 3.5f, 10, 10.5f, 1.5f);
            rrect (6, 2, 4, 3, 0.8f);
            strokeIt();
            return;
        case Icon::Erase:
            p.startNewSubPath (P (6, 13));
            p.lineTo (P (2.5f, 9.5f));
            p.lineTo (P (9, 3));
            p.lineTo (P (13.5f, 7.5f));
            p.lineTo (P (8, 13));
            p.closeSubPath();
            line (5.8f, 6.2f, 10.3f, 10.7f);
            line (8, 13, 13.5f, 13);
            strokeIt();
            return;
        case Icon::Bars:
            line (3, 3, 3, 13); line (13, 3, 13, 13); line (8, 3, 8, 13);
            strokeIt();
            return;
        case Icon::Menu:
            line (3, 4.5f, 13, 4.5f); line (3, 8, 13, 8); line (3, 11.5f, 13, 11.5f);
            strokeIt();
            return;
    }
}

IconButton::IconButton (Icon i, const juce::String& text) : juce::Button (text), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::setGlyph (const juce::String& g, float size, float yOffset)
{
    glyph = g;
    glyphSize = size;
    glyphY = yOffset;
    repaint();
}

int IconButton::idealWidth() const
{
    int w = 14;
    if (icon != Icon::None || glyph.isNotEmpty()) w += 16;
    if (getButtonText().isNotEmpty())
        w += juce::roundToInt (juce::GlyphArrangement::getStringWidth (HouseLook::uiFont (12.5f), getButtonText())) + (w > 14 ? 6 : 0);
    if (! swatch.isTransparent()) w += 14;
    return std::max (w, 30);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto* look = dynamic_cast<HouseLook*> (&getLookAndFeel());
    if (look == nullptr) return;
    const bool on = getToggleState();
    look->drawFace (g, getLocalBounds().toFloat(), on, highlighted, down, isEnabled(), isConnectedOnLeft(), isConnectedOnRight());
    const bool toggle = toggleLook || getClickingTogglesState() || getRadioGroupId() != 0;
    const auto ink = look->faceInk (on, toggle, isEnabled());
    g.setColour (ink);

    auto r = getLocalBounds().toFloat().withTrimmedBottom (1.0f).translated (0, down ? 0.5f : 0.0f);
    const auto text = getButtonText();
    const auto font = HouseLook::uiFont (12.5f);
    const float textW = text.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, text);
    const bool hasIcon = icon != Icon::None || glyph.isNotEmpty();
    float glyphW = 16.0f;
    juce::Font musicFont (juce::FontOptions (MusicFont::typeface()).withPointHeight (glyphSize > 0 ? glyphSize : 20.0f));
    if (glyph.isNotEmpty()) glyphW = std::max (12.0f, juce::GlyphArrangement::getStringWidth (musicFont, glyph));
    const float swatchW = swatch.isTransparent() ? 0.0f : 14.0f;
    const float total = (hasIcon ? glyphW : 0.0f) + (hasIcon && textW > 0 ? 6.0f : 0.0f) + textW + swatchW;
    float x = r.getCentreX() - total * 0.5f;

    if (glyph.isNotEmpty())
    {
        // fit the glyph's real ink box inside the button, centred
        juce::GlyphArrangement ga;
        ga.addLineOfText (musicFont, glyph, 0.0f, 0.0f);
        juce::Path path;
        ga.createPath (path);
        const auto ink = path.getBounds();
        const auto box = juce::Rectangle<float> (x, r.getY() + 5.0f, glyphW, r.getHeight() - 10.0f);
        if (! ink.isEmpty())
        {
            const float k = std::min (1.0f, std::min (box.getHeight() / ink.getHeight(), (box.getWidth() + 4.0f) / ink.getWidth()));
            path.applyTransform (juce::AffineTransform::translation (-ink.getCentreX(), -ink.getCentreY())
                                     .scaled (k)
                                     .translated (box.getCentreX(), box.getCentreY() + glyphY * 0.0f));
            g.fillPath (path);
        }
        x += glyphW + (textW > 0 ? 6.0f : 0.0f);
    }
    else if (icon != Icon::None)
    {
        drawIcon (g, icon, { x, r.getCentreY() - 8.0f, 16.0f, 16.0f });
        x += 16.0f + (textW > 0 ? 6.0f : 0.0f);
    }
    if (textW > 0)
    {
        g.setFont (font);
        g.drawText (text, juce::Rectangle<float> (x, r.getY(), textW + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
        x += textW;
    }
    if (swatchW > 0)
    {
        g.setColour (swatch);
        g.fillEllipse (x + 5.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawEllipse (x + 5.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f, 1.0f);
    }
}
