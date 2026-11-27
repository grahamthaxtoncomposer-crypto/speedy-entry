#include "StaffView.h"

#include "Glyphs.h"
#include "staffpaper/Layout.h"

#include <cmath>
#include <limits>
#include <set>

using namespace staffpaper;

namespace
{

int beamCount (int value) { return value <= 4 ? 5 - value : 0; } // 4 = eighth -> 1 beam ... 1 = 64th -> 4

juce::juce_wchar restGlyph (int value)
{
    switch (value)
    {
        case 8: return Glyph::restDoubleWhole;
        case 7: return Glyph::restWhole;
        case 6: return Glyph::restHalf;
        case 5: return Glyph::restQuarter;
        case 4: return Glyph::rest8th;
        case 3: return Glyph::rest16th;
        case 2: return Glyph::rest32nd;
        default: return Glyph::rest64th;
    }
}

juce::juce_wchar headGlyph (int value)
{
    if (value >= 8) return Glyph::noteheadDoubleWhole;
    if (value == 7) return Glyph::noteheadWhole;
    if (value == 6) return Glyph::noteheadHalf;
    return Glyph::noteheadBlack;
}

juce::juce_wchar accidentalGlyph (int alter)
{
    switch (alter)
    {
        case -2: return Glyph::doubleFlat;
        case -1: return Glyph::flat;
        case 1: return Glyph::sharp;
        case 2: return Glyph::doubleSharp;
        default: return Glyph::natural;
    }
}

// Staff position (relative to the middle line) of the clef's reference line.
int clefRefPos (Clef c)
{
    switch (c)
    {
        case Clef::Treble: case Clef::Treble8vb: case Clef::Treble8va: return -2;
        case Clef::Bass: case Clef::Bass8vb: case Clef::Bass8va: return 2;
        case Clef::Alto: return 0;
        case Clef::Tenor: return 2;
        case Clef::Soprano: return -4;
        case Clef::MezzoSoprano: return -2;
        case Clef::Baritone: return 4;
        case Clef::Percussion: return 0;
        default: return 0;
    }
}

juce::juce_wchar clefGlyph (Clef c)
{
    switch (c)
    {
        case Clef::Treble: return Glyph::gClef;
        case Clef::Treble8vb: return Glyph::gClef8vb;
        case Clef::Treble8va: return Glyph::gClef8va;
        case Clef::Bass: return Glyph::fClef;
        case Clef::Bass8vb: return Glyph::fClef8vb;
        case Clef::Bass8va: return Glyph::fClef8va;
        case Clef::Percussion: return Glyph::percussionClef;
        default: return Glyph::cClef;
    }
}

const char* kKeyNames[15] = { "Cb major / Ab minor", "Gb major / Eb minor", "Db major / Bb minor", "Ab major / F minor",
                              "Eb major / C minor", "Bb major / G minor", "F major / D minor", "C major / A minor",
                              "G major / E minor", "D major / B minor", "A major / F# minor", "E major / C# minor",
                              "B major / G# minor", "F# major / D# minor", "C# major / A# minor" };
} // namespace


StaffView::StaffView()
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setOpaque (true);
}

// ---------------------------------------------------------------- theme

StaffView::Theme StaffView::Theme::light (juce::Colour accent)
{
    return { juce::Colour (0xfff6f5f0), juce::Colour (0xff171916), juce::Colour (0xffb3b2aa), juce::Colour (0xffecebe4),
             juce::Colour (0xffd3d1c8), juce::Colour (0xff6b6d64), accent, juce::Colour (0xff4067d6) };
}

StaffView::Theme StaffView::Theme::dark (juce::Colour accent)
{
    return { juce::Colour (0xff171916), juce::Colour (0xfff2f0e9), juce::Colour (0xff5d6058), juce::Colour (0xff20231f),
             juce::Colour (0xff41443d), juce::Colour (0xffa4a69c), accent, juce::Colour (0xff6d8cf0) };
}

juce::Colour StaffView::Theme::mark() const
{
    // a light accent (like lime) would vanish on light paper: darken it until it reads
    juce::Colour c = accent;
    if (paper.getPerceivedBrightness() > 0.5f)
        for (int i = 0; i < 6 && c.getPerceivedBrightness() > 0.55f; ++i) c = c.darker (0.35f);
    else
        for (int i = 0; i < 6 && c.getPerceivedBrightness() < 0.45f; ++i) c = c.brighter (0.35f);
    return c;
}

void StaffView::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void StaffView::setScore (Score* s, Editor* e)
{
    score = s;
    editor = e;
    spacingDirty = true;
    repaint();
}

void StaffView::setPlayheadTick (double t)
{
    if (std::abs (t - playheadTick) < 1.0) return;
    playheadTick = t;
    repaint();
}

void StaffView::setStaffSpace (float s)
{
    space = juce::jlimit (5.0f, 20.0f, s);
    spacingDirty = true;
    repaint();
}

void StaffView::setTool (Tool t)
{
    tool = t;
    setMouseCursor (t == Tool::Select ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

juce::String StaffView::gridName (int g)
{
    static const char* names[] = { "Free", "128th", "64th", "32nd", "16th", "8th", "Quarter", "Half", "Whole", "Double whole", "Measure" };
    if (g >= 0 && g <= 10) return names[g];
    return "Measure";
}

Tick StaffView::gridTicks (int measure) const
{
    if (grid <= 0) return 0;
    if (grid >= 10) return score->measureCapacity (measure);
    return (PPQ / 32) << (grid - 1); // 1 = 128th
}

bool StaffView::layerDimmed (int layer) const
{
    // like the classic programs: while entering, the other layers are greyed
    return editor && editor->frameActive() && layer != editor->cursor().layer;
}

juce::Rectangle<float> StaffView::laneArea() const
{
    return getLocalBounds().toFloat().removeFromBottom (laneHeight());
}

// ---------------------------------------------------------------- layout

StaffView::MeasureGeom StaffView::layoutMeasure (int m, float x, bool sysStart)
{
    const int nStaves = (int) score->staves.size();
    const Cursor cur = editor ? editor->cursor() : Cursor {};
    MeasureGeom mg;
    mg.x = x;
    mg.systemStart = sysStart;
    mg.frame = (editor != nullptr && editor->frameActive() && m == cur.measure);
    const float scale = mg.frame ? 1.7f : 1.0f;
    const auto& info = score->measures[(size_t) m];

    // ---- header: clef, key, time
    float hx = space * 0.8f;
    bool clefHere = sysStart;
    for (int s = 0; s < nStaves; ++s)
        if (score->staves[(size_t) s].measures[(size_t) m].clefChange) clefHere = true;
    if (clefHere) hx += (sysStart ? clefWidth() : space * 2.8f);
    const bool keyHere = (sysStart && score->keyAt (m) != 0) || info.keyFifths.has_value();
    if (keyHere) hx += (float) std::abs (score->keyAt (m)) * space * 1.0f + (score->keyAt (m) != 0 ? space * 0.6f : 0.0f);
    const bool timeHere = (m == 0) || info.timeSig.has_value();
    if (timeHere) hx += space * 2.8f;
    if (info.repeatStart) hx += space * 1.4f;
    hx += space * 1.0f;

    // ---- columns across staves and layers
    struct Key
    {
        Tick tick;
        int order;
        bool operator< (const Key& o) const { return tick != o.tick ? tick < o.tick : order < o.order; }
    };
    std::map<Key, float> pre, post, spacing;
    std::vector<std::array<std::vector<Key>, kLayers>> entryKeys ((size_t) nStaves);
    const Tick cap = score->measureCapacity (m);

    for (int s = 0; s < nStaves; ++s)
        for (int l = 0; l < kLayers; ++l)
        {
            const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
            const auto onsets = entryOnsets (es);
            const int n = (int) es.size();
            for (int i = 0; i < n; ++i)
            {
                const auto& e = es[(size_t) i];
                int order = 0;
                if (e.grace)
                {
                    int remaining = 0;
                    for (int j = i; j < n && es[(size_t) j].grace; ++j) ++remaining;
                    order = -remaining;
                }
                Key k { onsets[(size_t) i], order };
                entryKeys[(size_t) s][(size_t) l].push_back (k);
                float pr = 0;
                for (int ni = 0; ni < (int) e.notes.size(); ++ni)
                    if (score->displayedAccidental (s, m, i, ni, l)) pr = std::max (pr, space * (e.notes[(size_t) ni].parentheses ? 2.2f : 1.4f));
                if (e.clefBefore) pr += space * 3.0f;
                pre[k] = std::max (pre[k], pr);
                post[k] = std::max (post[k], (float) e.dots * space * 0.7f);
                float w;
                if (e.grace)
                    w = space * 1.7f;
                else
                {
                    const double d = (double) std::max<Tick> (e.duration(), PPQ / 16);
                    w = space * (1.3f + 2.3f * (float) std::sqrt (d / (double) PPQ)) * scale;
                }
                auto it = spacing.find (k);
                spacing[k] = (it == spacing.end()) ? w : std::min (it->second, w);
            }
        }

    float cx = x + hx;
    mg.firstNoteX = cx;
    std::map<Key, float> colX;
    for (auto& [k, w] : spacing)
    {
        cx += pre[k];
        colX[k] = cx;
        if (k.order == 0) mg.tickX.push_back ({ k.tick, cx });
        cx += w + post[k];
    }
    if (spacing.empty()) cx += space * 7.0f * scale;
    cx += space * 0.6f;
    if (info.repeatEnd) cx += space * 1.4f;
    mg.width = std::max (cx - x, space * 6.0f);
    if (mg.tickX.empty() || mg.tickX.front().first > 0) mg.tickX.insert (mg.tickX.begin(), { 0, mg.firstNoteX });
    mg.tickX.push_back ({ cap, x + mg.width });

    mg.entries.resize ((size_t) nStaves);
    for (int s = 0; s < nStaves; ++s)
        for (int l = 0; l < kLayers; ++l)
            for (const auto& k : entryKeys[(size_t) s][(size_t) l])
            {
                EntryGeom eg;
                eg.x = colX[k];
                mg.entries[(size_t) s][(size_t) l].push_back (eg);
            }
    return mg;
}

void StaffView::computeSpacing()
{
    // How far music reaches above and below each staff, in staff steps from the middle line.
    const int nStaves = (int) score->staves.size();
    std::vector<int> above ((size_t) nStaves, 4), below ((size_t) nStaves, 4);
    std::vector<bool> dyn ((size_t) nStaves, false);
    for (int s = 0; s < nStaves; ++s)
        for (int m = 0; m < score->numMeasures(); ++m)
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
                for (int i = 0; i < (int) es.size(); ++i)
                {
                    const auto& e = es[(size_t) i];
                    if (e.dynamic) dyn[(size_t) s] = true;
                    if (e.rest || e.notes.empty()) continue;
                    const Clef c = score->clefAtEntry (s, m, i, l);
                    const int hi = staffPosition (e.notes.back().pitch.dia, c);
                    const int lo = staffPosition (e.notes.front().pitch.dia, c);
                    const bool up = autoStemUp (e, c, layerStemRule (score->staves[(size_t) s].measures[(size_t) m], l));
                    const int stem = e.value <= 6 ? 7 : 0;
                    const int marks = 2 * ((e.staccato ? 1 : 0) + (e.tenuto ? 1 : 0) + (e.accent ? 1 : 0));
                    above[(size_t) s] = std::max (above[(size_t) s], hi + (up ? stem : marks) + 1);
                    below[(size_t) s] = std::max (below[(size_t) s], -lo + (up ? marks : stem) + 1);
                }
            }
    if (editor && editor->frameActive())
    {
        // leave room for the crossbar where it is
        const auto c = editor->cursor();
        const int pos = staffPosition (c.dia, score->clefAtEntry (c.staff, c.measure, c.entry, c.layer));
        above[(size_t) c.staff] = std::max (above[(size_t) c.staff], pos + 2);
        below[(size_t) c.staff] = std::max (below[(size_t) c.staff], -pos + 2);
    }

    middleY.assign ((size_t) nStaves, 0.0f);
    const float top = space * 4.0f + std::max (8.0f, (float) above[0] * 0.5f + 2.0f) * space * 0.5f + space * 2.5f;
    middleY[0] = top;
    for (int s = 1; s < nStaves; ++s)
    {
        const float need = ((float) below[(size_t) s - 1] + (float) above[(size_t) s]) * 0.5f * space + space * (dyn[(size_t) s - 1] ? 5.0f : 3.0f);
        middleY[(size_t) s] = middleY[(size_t) s - 1] + std::max (space * 13.0f, need);
    }
    stavesBottom = middleY.back() + std::max (space * 6.0f, (float) below.back() * 0.5f * space + space * (dyn.back() ? 5.5f : 3.0f));
    spacingDirty = false;
    spacingMeasure = editor ? editor->cursor().measure : -1;
    spacingFrame = editor ? editor->frameActive() : false;
}

void StaffView::computeLayout()
{
    geo = {};
    if (! score) return;
    const int nStaves = (int) score->staves.size();
    // Staves move apart only when the entry frame moves on or closes, so they
    // stay still while you type.
    const bool frameNow = editor ? editor->frameActive() : false;
    const int measureNow = editor ? editor->cursor().measure : -1;
    if (spacingDirty || (int) middleY.size() != nStaves || measureNow != spacingMeasure || frameNow != spacingFrame)
        computeSpacing();

    const float left = space * (nStaves > 1 ? 3.0f : 1.5f);
    float x = left;
    for (int m = 0; m < score->numMeasures(); ++m)
    {
        auto mg = layoutMeasure (m, x, m == 0);
        x += mg.width;
        geo.measures.push_back (std::move (mg));
    }
    geo.totalWidth = x + space * 4.0f;
    geo.totalHeight = stavesBottom;

    velocities = computeVelocities (*score);
    for (int m = 0; m < (int) geo.measures.size(); ++m)
        for (int s = 0; s < nStaves; ++s)
            for (int l = 0; l < kLayers; ++l)
                computeStems (s, m, l);
}

void StaffView::computeStems (int s, int m, int l)
{
    auto& mg = geo.measures[(size_t) m];
    const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
    const auto& es = sm.layers[(size_t) l];
    auto& egs = mg.entries[(size_t) s][(size_t) l];
    const float hw = headWidth();
    const int rule = layerStemRule (sm, l);

    auto noteY = [&] (const Entry& e, int idx, int entryIndex) {
        const Clef c = score->clefAtEntry (s, m, entryIndex, l);
        return yForPos (s, staffPosition (e.notes[(size_t) idx].pitch.dia, c));
    };

    for (int i = 0; i < (int) es.size(); ++i)
    {
        const auto& e = es[(size_t) i];
        auto& eg = egs[(size_t) i];
        if (e.rest || e.notes.empty()) continue;
        const float scale = e.grace ? 0.65f : 1.0f;
        eg.stemUp = autoStemUp (e, score->clefAtEntry (s, m, i, l), rule);
        eg.stemX = eg.stemUp ? eg.x + hw * scale - space * 0.06f : eg.x + space * 0.06f;
        const float top = noteY (e, (int) e.notes.size() - 1, i);
        const float bot = noteY (e, 0, i);
        const float len = space * 3.5f * scale;
        eg.stemEndY = eg.stemUp ? top - len : bot + len;
        // stems reach the middle line when notes are far away (one layer only)
        const float midY = mid (s);
        if (rule == 0 && eg.stemUp && eg.stemEndY > midY && ! e.grace) eg.stemEndY = midY;
        if (rule == 0 && ! eg.stemUp && eg.stemEndY < midY && ! e.grace) eg.stemEndY = midY;
    }

    for (const auto& group : beamGroups (*score, s, m, l))
    {
        const bool up = groupStemUp (es, group, score->clefAtEntry (s, m, group.front(), l), rule);
        const int maxBeams = [&] { int b = 1; for (int i : group) b = std::max (b, beamCount (es[(size_t) i].value)); return b; }();
        const float minLen = space * (2.5f + 0.75f * (float) (maxBeams - 1));
        std::vector<float> ext; // furthest notehead in stem direction
        bool flat = false;
        for (int i : group)
        {
            const auto& e = es[(size_t) i];
            auto& eg = egs[(size_t) i];
            eg.stemUp = up;
            eg.beamed = true;
            eg.stemX = up ? eg.x + hw - space * 0.06f : eg.x + space * 0.06f;
            ext.push_back (up ? noteY (e, (int) e.notes.size() - 1, i) : noteY (e, 0, i));
            flat = flat || e.flatBeam;
        }
        const auto& first = egs[(size_t) group.front()];
        const auto& last = egs[(size_t) group.back()];
        const float dx = std::max (1.0f, last.stemX - first.stemX);
        float slope = flat ? 0.0f : (ext.back() - ext.front()) / dx;
        slope = juce::jlimit (-space * 1.0f / dx, space * 1.0f / dx, slope);
        float offset = up ? 1e9f : -1e9f;
        for (size_t k = 0; k < group.size(); ++k)
        {
            const float sx = egs[(size_t) group[k]].stemX;
            const float lineY = slope * (sx - first.stemX);
            const float need = up ? ext[k] - minLen - lineY : ext[k] + minLen - lineY;
            offset = up ? std::min (offset, need) : std::max (offset, need);
        }
        for (int i : group)
        {
            auto& eg = egs[(size_t) i];
            eg.stemEndY = offset + slope * (eg.stemX - first.stemX);
        }
    }
}

void StaffView::ensureCursorVisible()
{
    if (! editor || geo.measures.empty()) return;
    const auto& mg = geo.measures[(size_t) juce::jlimit (0, (int) geo.measures.size() - 1, editor->cursor().measure)];
    const float w = (float) getWidth();
    if (mg.x - scrollX < space * 2)
        scrollX = std::max (0.0f, mg.x - space * 4);
    else if (mg.x + mg.width - scrollX > w - space * 2)
        scrollX = std::max (0.0f, std::min (mg.x - space * 4, mg.x + mg.width - w + space * 6));
    // keep the cursor staff on screen vertically
    const float visibleH = (float) getHeight() - laneHeight();
    const float y = mid (editor->cursor().staff);
    if (y - space * 8 - scrollY < 0) scrollY = std::max (0.0f, y - space * 8);
    else if (y + space * 8 - scrollY > visibleH) scrollY = std::max (0.0f, y + space * 8 - visibleH);
    scrollY = juce::jlimit (0.0f, std::max (0.0f, geo.totalHeight - visibleH), scrollY);
}

// ---------------------------------------------------------------- painting

void StaffView::glyph (juce::Graphics& g, juce::juce_wchar c, float x, float y, float scale)
{
    g.setFont (MusicFont::forStaffSpace (space * scale));
    g.drawSingleLineText (juce::String::charToString (c), juce::roundToInt (x), juce::roundToInt (y));
}

void StaffView::paint (juce::Graphics& g)
{
    g.fillAll (theme.paper);
    if (! score) return;
    computeLayout();
    if (followCursor) ensureCursorVisible();
    const int last = (int) middleY.size() - 1;

    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedBottom ((int) laneHeight()));
    g.addTransform (juce::AffineTransform::translation (-scrollX, -scrollY));

    // frame behind everything
    if (editor && editor->frameActive())
    {
        const int cm = editor->cursor().measure;
        const auto& mg = geo.measures[(size_t) cm];
        const float top = mid (0) - space * 6.0f;
        const float bottom = mid (last) + space * 6.0f;
        g.setColour (theme.mark().withAlpha (0.07f));
        g.fillRoundedRectangle (juce::Rectangle<float> (mg.x, top, mg.width, bottom - top), space * 0.6f);
        g.setColour (theme.mark());
        g.drawRoundedRectangle (juce::Rectangle<float> (mg.x, top, mg.width, bottom - top), space * 0.6f, 1.5f);
    }
    drawSelection (g);

    const juce::Rectangle<float> visible (scrollX - space * 20, scrollY - space * 20, (float) getWidth() + space * 40, (float) getHeight() + space * 40);
    if (! geo.measures.empty())
    {
        const float x0 = geo.measures.front().x, x1 = geo.measures.back().x + geo.measures.back().width;
        g.setColour (theme.ink);
        drawStaffLines (g, x0, x1);
        const float top = mid (0) - space * 2.0f;
        const float bottom = mid (last) + space * 2.0f;
        g.fillRect (juce::Rectangle<float> (x0 - space * 0.08f, top, space * 0.16f, bottom - top));
        if (score->staves.size() > 1)
        {
            // the brace glyph is one staff tall: stretch it vertically to span the system
            const float k = (bottom - top) / (space * 4.0f);
            const float bx = x0 - space * 1.3f;
            g.saveState();
            g.addTransform (juce::AffineTransform::scale (0.6f + 0.4f * k, k, bx + space, bottom));
            glyph (g, Glyph::brace, bx, bottom);
            g.restoreState();
        }
        for (int m = 0; m < (int) geo.measures.size(); ++m)
        {
            const auto& mg = geo.measures[(size_t) m];
            if (mg.x + mg.width < visible.getX() || mg.x > visible.getRight()) continue;
            drawMeasure (g, m);
        }
    }
    drawTiesAndSlurs (g);
    drawCursor (g);
    drawPlayhead (g);

    if (drag == Drag::Select)
    {
        const auto r = juce::Rectangle<float> (dragStart, dragNow).translated (scrollX, scrollY);
        g.setColour (theme.mark().withAlpha (0.12f));
        g.fillRect (r);
        g.setColour (theme.mark().withAlpha (0.7f));
        g.drawRect (r, 1.0f);
    }
    g.restoreState();

    if (! (editor && editor->frameActive()))
    {
        g.setColour (theme.muted);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (tool == Tool::Select ? "Select: drag over music, Cmd+C / Cmd+V" : "Click a measure or press Esc to enter notes",
                    getLocalBounds().withTrimmedBottom ((int) laneHeight()).reduced (10, 6), juce::Justification::topRight);
    }
    drawLane (g);
}

void StaffView::drawStaffLines (juce::Graphics& g, float x0, float x1)
{
    const float t = std::max (1.0f, space * 0.11f);
    for (size_t s = 0; s < middleY.size(); ++s)
        for (int l = -2; l <= 2; ++l)
        {
            const float y = mid ((int) s) - (float) l * space;
            g.fillRect (juce::Rectangle<float> (x0, y - t * 0.5f, x1 - x0, t));
        }
}

void StaffView::drawClef (juce::Graphics& g, Clef c, int staff, float x, bool small)
{
    glyph (g, clefGlyph (c), x, yForPos (staff, clefRefPos (c)), small ? 0.75f : 1.0f);
}

void StaffView::drawKeySig (juce::Graphics& g, int fifths, Clef c, int staff, float x)
{
    if (fifths == 0 || c == Clef::Percussion) return;
    static const int sharps[7] = { 3, 0, 4, 1, 5, 2, 6 };
    static const int flats[7] = { 6, 2, 5, 1, 4, 0, 3 };
    const int middle = clefMiddleLine (c);
    const int lo = fifths > 0 ? -1 : -3;
    for (int i = 0; i < std::min (7, std::abs (fifths)); ++i)
    {
        const int step = fifths > 0 ? sharps[i] : flats[i];
        int pos = ((step - middle) % 7 + 7) % 7; // 0..6
        while (pos > lo + 6) pos -= 7;
        while (pos < lo) pos += 7;
        glyph (g, fifths > 0 ? Glyph::sharp : Glyph::flat, x + (float) i * space * 1.0f, yForPos (staff, pos));
    }
}

void StaffView::drawTimeSig (juce::Graphics& g, const TimeSig& t, int staff, float x)
{
    auto digits = [&] (int v, int pos) {
        const auto str = juce::String (v);
        const float w = space * 1.8f * (float) str.length();
        float dx = x + space * 1.1f - w * 0.5f;
        for (auto ch : str)
        {
            glyph (g, (juce::juce_wchar) (Glyph::timeSig0 + (ch - '0')), dx, yForPos (staff, pos));
            dx += space * 1.8f;
        }
    };
    digits (t.num, 2);
    digits (t.den, -2);
}

void StaffView::drawBarline (juce::Graphics& g, int m, float x)
{
    const auto& info = score->measures[(size_t) m];
    const float top = mid (0) - space * 2.0f;
    const float bottom = mid ((int) middleY.size() - 1) + space * 2.0f;
    const float thin = std::max (1.0f, space * 0.16f), thick = space * 0.5f;

    auto line = [&] (float lx, float w) { g.fillRect (juce::Rectangle<float> (lx - w, top, w, bottom - top)); };
    auto dots = [&] (float dx) {
        for (size_t s = 0; s < middleY.size(); ++s)
            for (int p : { 1, -1 })
                g.fillEllipse (dx - space * 0.25f, yForPos ((int) s, p) - space * 0.25f, space * 0.5f, space * 0.5f);
    };

    if (info.repeatEnd)
    {
        line (x, thick);
        line (x - thick - space * 0.4f, thin);
        dots (x - thick - space * 1.0f);
        return;
    }
    switch (info.barline)
    {
        case Barline::Single: line (x, thin); break;
        case Barline::Double: line (x, thin); line (x - space * 0.5f, thin); break;
        case Barline::Final: line (x, thick); line (x - thick - space * 0.4f, thin); break;
        case Barline::Thick: line (x, thick); break;
        case Barline::Dashed:
            for (float y = top; y < bottom; y += space * 0.8f)
                g.fillRect (juce::Rectangle<float> (x - thin, y, thin, std::min (space * 0.5f, bottom - y)));
            break;
        case Barline::Dotted:
            for (float y = top + space * 0.5f; y < bottom; y += space)
                g.fillEllipse (x - space * 0.15f, y - space * 0.1f, space * 0.25f, space * 0.25f);
            break;
        case Barline::Invisible:
        default: break;
    }
}

void StaffView::drawMeasure (juce::Graphics& g, int m)
{
    const auto& mg = geo.measures[(size_t) m];
    const auto& info = score->measures[(size_t) m];
    const int nStaves = (int) score->staves.size();

    // measure number
    g.setColour (theme.muted);
    g.setFont (juce::FontOptions (space * 1.2f));
    g.drawText (juce::String (m + 1), juce::Rectangle<float> (mg.x + 2, mid (0) - space * 5.0f, space * 4, space * 1.5f),
                juce::Justification::centredLeft);
    g.setColour (theme.ink);

    // header
    const bool sysStart = mg.systemStart;
    float hx = mg.x + space * 0.8f;
    bool clefHere = sysStart;
    for (int s = 0; s < nStaves; ++s)
        if (score->staves[(size_t) s].measures[(size_t) m].clefChange) clefHere = true;
    if (clefHere)
    {
        for (int s = 0; s < nStaves; ++s)
            if (sysStart || score->staves[(size_t) s].measures[(size_t) m].clefChange)
                drawClef (g, score->clefAt (s, m), s, hx, ! sysStart);
        hx += (sysStart ? clefWidth() : space * 2.8f);
    }
    if ((sysStart && score->keyAt (m) != 0) || info.keyFifths)
    {
        const int k = score->keyAt (m);
        for (int s = 0; s < nStaves; ++s)
            drawKeySig (g, k, score->clefAt (s, m), s, hx);
        hx += (float) std::abs (k) * space * 1.0f + (k != 0 ? space * 0.6f : 0.0f);
    }
    if ((m == 0) || info.timeSig)
    {
        for (int s = 0; s < nStaves; ++s)
            drawTimeSig (g, score->timeSigAt (m), s, hx);
        hx += space * 2.8f;
    }
    if (info.repeatStart)
    {
        const float top = mid (0) - space * 2.0f, bottom = mid ((int) middleY.size() - 1) + space * 2.0f;
        g.fillRect (juce::Rectangle<float> (hx - space * 0.6f, top, space * 0.5f, bottom - top));
        g.fillRect (juce::Rectangle<float> (hx + space * 0.2f, top, std::max (1.0f, space * 0.16f), bottom - top));
        for (int s = 0; s < nStaves; ++s)
            for (int p : { 1, -1 })
                g.fillEllipse (hx + space * 0.7f, yForPos (s, p) - space * 0.25f, space * 0.5f, space * 0.5f);
    }

    // ending bracket
    if (info.endingMask != 0)
    {
        const float y = mid (0) - space * 6.5f;
        juce::String label;
        for (int b = 0; b < 8; ++b)
            if (info.endingMask & (1u << b)) label << (label.isEmpty() ? "" : ", ") << (b + 1);
        g.drawLine (mg.x + space * 0.3f, y, mg.x + mg.width - space * 0.3f, y, std::max (1.0f, space * 0.13f));
        const bool startsHere = m == 0 || score->measures[(size_t) m - 1].endingMask != info.endingMask;
        if (startsHere)
        {
            g.drawLine (mg.x + space * 0.3f, y, mg.x + space * 0.3f, y + space * 2.0f, std::max (1.0f, space * 0.13f));
            g.setFont (juce::FontOptions (space * 1.4f));
            g.drawText (label + ".", juce::Rectangle<float> (mg.x + space * 0.7f, y, space * 6, space * 1.8f), juce::Justification::centredLeft);
        }
    }

    for (int s = 0; s < nStaves; ++s)
        for (int l = kLayers - 1; l >= 0; --l)
        {
            for (int i = 0; i < (int) score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l].size(); ++i)
                drawEntry (g, s, m, l, i);
            drawBeams (g, s, m, l);
            drawTuplets (g, s, m, l);
        }

    // overfull measure warning
    for (int s = 0; s < nStaves; ++s)
        for (int l = 0; l < kLayers; ++l)
            if (score->contentLength (s, m, l) > score->measureCapacity (m))
            {
                g.setColour (juce::Colour (0xffd23c3c).withAlpha (0.2f));
                g.fillRect (juce::Rectangle<float> (mg.x, mid (s) - space * 2, mg.width, space * 4));
            }

    g.setColour (theme.ink);
    drawBarline (g, m, mg.x + mg.width);
}

juce::Colour StaffView::entryColour (int s, int m, int l, int i, const Entry& e) const
{
    if (editor && editor->selection().active)
    {
        const Tick t = score->measureStart (m) + score->onsetOf (s, m, l, i);
        if (editor->selection().contains (s, t)) return theme.mark();
    }
    if (e.hidden) return theme.hidden;
    if (layerDimmed (l)) return theme.ink.withAlpha (0.42f);
    return theme.ink;
}

void StaffView::drawEntry (juce::Graphics& g, int s, int m, int l, int i)
{
    const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
    const auto& e = sm.layers[(size_t) l][(size_t) i];
    const auto& eg = geo.measures[(size_t) m].entries[(size_t) s][(size_t) l][(size_t) i];
    const Clef clef = score->clefAtEntry (s, m, i, l);
    const float sc = e.grace ? 0.65f : 1.0f;
    const float hw = headWidth() * sc;
    const int rule = layerStemRule (sm, l);
    g.setColour (entryColour (s, m, l, i, e));

    if (e.clefBefore)
    {
        // pre-space includes the clef; draw it just left of any accidentals
        float clefX = eg.x - space * 3.0f;
        for (int ni = 0; ni < (int) e.notes.size(); ++ni)
            if (score->displayedAccidental (s, m, i, ni, l)) { clefX -= space * 1.4f; break; }
        drawClef (g, *e.clefBefore, s, clefX, true);
    }

    if (e.dynamic)
    {
        // under the staff, or under the lowest note when it hangs below
        float y = mid (s) + space * 4.6f;
        if (! e.rest && ! e.notes.empty())
            y = std::max (y, yForPos (s, staffPosition (e.notes.front().pitch.dia, clef)) + space * 3.2f);
        glyph (g, Glyph::dynamics[(int) *e.dynamic], eg.x - space * 0.4f, y);
    }

    if (e.rest)
    {
        int pos = 0;
        if (e.value == 7) pos = 2;
        if (rule > 0) pos += 4;
        if (rule < 0) pos -= 4;
        const bool wholeMeasureRest = sm.layers[(size_t) l].size() == 1 && e.value == 7;
        float rx = eg.x;
        if (wholeMeasureRest)
        {
            const auto& mg = geo.measures[(size_t) m];
            rx = (mg.firstNoteX + mg.x + mg.width) * 0.5f - hw * 0.5f;
        }
        glyph (g, restGlyph (e.value), rx, yForPos (s, pos), sc);
        for (int d = 0; d < e.dots; ++d)
            g.fillEllipse (rx + space * (1.6f + 0.6f * (float) d), yForPos (s, pos + 1) - space * 0.2f, space * 0.4f, space * 0.4f);
        return;
    }

    // noteheads, seconds displaced to the other side of the stem
    std::vector<float> headX (e.notes.size(), eg.x);
    if (eg.stemUp)
    {
        for (size_t k = 1; k < e.notes.size(); ++k)
            if (e.notes[k].pitch.dia - e.notes[k - 1].pitch.dia == 1 && headX[k - 1] == eg.x) headX[k] = eg.x + hw;
    }
    else
    {
        for (int k = (int) e.notes.size() - 2; k >= 0; --k)
            if (e.notes[(size_t) k + 1].pitch.dia - e.notes[(size_t) k].pitch.dia == 1 && headX[(size_t) k + 1] == eg.x)
                headX[(size_t) k] = eg.x - hw;
    }

    int accCol = 0, lastAccPos = 1000;
    for (int k = (int) e.notes.size() - 1; k >= 0; --k)
    {
        const auto& n = e.notes[(size_t) k];
        const int pos = staffPosition (n.pitch.dia, clef);
        const float y = yForPos (s, pos);
        glyph (g, headGlyph (e.value), headX[(size_t) k], y, sc);

        // ledger lines
        const float lw = std::max (1.0f, space * 0.16f);
        for (int ll = 6; ll <= pos; ll += 2)
            g.fillRect (juce::Rectangle<float> (headX[(size_t) k] - space * 0.4f, yForPos (s, ll) - lw * 0.5f, hw + space * 0.8f, lw));
        for (int ll = -6; ll >= pos; ll -= 2)
            g.fillRect (juce::Rectangle<float> (headX[(size_t) k] - space * 0.4f, yForPos (s, ll) - lw * 0.5f, hw + space * 0.8f, lw));

        if (auto acc = score->displayedAccidental (s, m, i, k, l))
        {
            if (lastAccPos - pos < 6) ++accCol; else accCol = 0;
            lastAccPos = pos;
            float ax = std::min (eg.x, headX[(size_t) k]) - space * (1.3f + 1.1f * (float) accCol) * sc;
            if (n.parentheses)
            {
                glyph (g, Glyph::parenRight, ax + space * 0.9f * sc, y, sc);
                glyph (g, accidentalGlyph (*acc), ax, y, sc);
                glyph (g, Glyph::parenLeft, ax - space * 0.6f * sc, y, sc);
            }
            else
                glyph (g, accidentalGlyph (*acc), ax, y, sc);
        }

        // dots sit in a space
        const int dotPos = (pos % 2 == 0) ? pos + 1 : pos;
        float dx = std::max (eg.x, headX[(size_t) k]) + hw + space * 0.45f;
        if (eg.stemUp && e.value <= 4 && ! eg.beamed) dx += space * 0.8f; // clear the flag
        for (int d = 0; d < e.dots; ++d)
            g.fillEllipse (dx + space * 0.6f * (float) d, yForPos (s, dotPos) - space * 0.2f, space * 0.4f, space * 0.4f);
    }

    // stem and flag
    if (e.value <= 6)
    {
        const float yTop = yForPos (s, staffPosition (e.notes.back().pitch.dia, clef));
        const float yBot = yForPos (s, staffPosition (e.notes.front().pitch.dia, clef));
        const float stemW = std::max (1.0f, space * 0.12f);
        if (eg.stemUp)
            g.fillRect (juce::Rectangle<float> (eg.stemX - stemW, eg.stemEndY, stemW, yBot - eg.stemEndY));
        else
            g.fillRect (juce::Rectangle<float> (eg.stemX, yTop, stemW, eg.stemEndY - yTop));
        const int beams = beamCount (e.value);
        if (beams > 0 && ! eg.beamed)
            glyph (g, eg.stemUp ? Glyph::flagUp[beams - 1] : Glyph::flagDown[beams - 1],
                   eg.stemUp ? eg.stemX - stemW : eg.stemX, eg.stemEndY, sc);
        if (e.grace && e.graceSlash)
        {
            // slash through the stem
            const float sy = eg.stemUp ? eg.stemEndY + space * 1.2f : eg.stemEndY - space * 1.2f;
            g.drawLine (eg.stemX - space * 0.7f, sy + space * 0.6f, eg.stemX + space * 0.9f, sy - space * 0.6f, stemW);
        }
    }

    // articulations on the notehead side, away from the stem
    {
        const bool above = ! eg.stemUp;
        const int endPos = staffPosition ((above ? e.notes.back() : e.notes.front()).pitch.dia, clef);
        int pos = above ? endPos + 2 : endPos - 2;
        if (pos % 2 == 0) pos += above ? 1 : -1; // in a space
        const float cx = eg.x + hw * 0.5f - space * 0.5f;
        auto place = [&] (juce::juce_wchar up, juce::juce_wchar down) {
            glyph (g, above ? up : down, cx, yForPos (s, pos), sc);
            pos += above ? 2 : -2;
        };
        if (e.staccato) place (Glyph::staccatoAbove, Glyph::staccatoBelow);
        if (e.tenuto) place (Glyph::tenutoAbove, Glyph::tenutoBelow);
        if (e.accent) place (Glyph::accentAbove, Glyph::accentBelow);
    }
}

void StaffView::drawBeams (juce::Graphics& g, int s, int m, int l)
{
    const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
    const auto& egs = geo.measures[(size_t) m].entries[(size_t) s][(size_t) l];
    for (const auto& group : beamGroups (*score, s, m, l))
    {
        const bool up = egs[(size_t) group.front()].stemUp;
        const float thick = space * 0.5f, gap = space * 0.75f;
        const float sw = std::max (1.0f, space * 0.12f);
        auto beamAt = [&] (size_t a, size_t b, int level) {
            const auto& ea = egs[(size_t) group[a]];
            const auto& eb = egs[(size_t) group[b]];
            const float off = (float) level * gap * (up ? 1.0f : -1.0f);
            const float xa = up ? ea.stemX - sw : ea.stemX, xb = up ? eb.stemX : eb.stemX + sw;
            juce::Path p;
            const float ya = ea.stemEndY + off, yb = eb.stemEndY + off;
            const float t = up ? thick : -thick;
            p.startNewSubPath (xa, ya);
            p.lineTo (xb, yb);
            p.lineTo (xb, yb + t);
            p.lineTo (xa, ya + t);
            p.closeSubPath();
            g.setColour (entryColour (s, m, l, group[a], es[(size_t) group[a]]));
            g.fillPath (p);
        };
        beamAt (0, group.size() - 1, 0);
        for (int level = 1; level < 4; ++level)
        {
            for (size_t k = 0; k < group.size(); ++k)
            {
                const bool here = beamCount (es[(size_t) group[k]].value) > level;
                if (! here) continue;
                const bool next = k + 1 < group.size() && beamCount (es[(size_t) group[k + 1]].value) > level;
                const bool prev = k > 0 && beamCount (es[(size_t) group[k - 1]].value) > level;
                if (next) beamAt (k, k + 1, level);
                else if (! prev)
                {
                    // partial beam (stub)
                    const auto& ea = egs[(size_t) group[k]];
                    const float dir = (k + 1 < group.size()) ? 1.0f : -1.0f;
                    const float off = (float) level * gap * (up ? 1.0f : -1.0f);
                    const float x = up ? ea.stemX - sw : ea.stemX;
                    const float len = space * 1.1f * dir;
                    g.fillRect (juce::Rectangle<float> (std::min (x, x + len), ea.stemEndY + off + (up ? 0 : -thick), std::abs (len), thick));
                }
            }
        }
    }
}

void StaffView::drawTuplets (juce::Graphics& g, int s, int m, int l)
{
    const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
    const auto& egs = geo.measures[(size_t) m].entries[(size_t) s][(size_t) l];
    for (int i = 0; i < (int) es.size(); ++i)
    {
        const auto& e = es[(size_t) i];
        if (! e.tupletStart || ! e.tuplet.active()) continue;
        int j = i;
        while (j + 1 < (int) es.size() && es[(size_t) j + 1].tuplet.active() && ! es[(size_t) j + 1].tupletStart
               && j - i + 1 < e.tuplet.actual)
            ++j;
        // the bracket goes on the stem side; for rests use the layer's side
        bool up = egs[(size_t) i].stemUp;
        for (int k = i; k <= j; ++k)
            if (! es[(size_t) k].rest) { up = egs[(size_t) k].stemUp; break; }
        float y = up ? mid (s) - space * 2.5f : mid (s) + space * 2.5f;
        for (int k = i; k <= j; ++k)
        {
            const auto& eg = egs[(size_t) k];
            const auto& ek = es[(size_t) k];
            if (ek.rest || ek.value >= 7) continue;
            const Clef c = score->clefAtEntry (s, m, k, l);
            const float headY = yForPos (s, staffPosition ((up ? ek.notes.back() : ek.notes.front()).pitch.dia, c));
            const float ext = ek.value <= 6 ? eg.stemEndY : headY;
            y = up ? std::min (y, std::min (ext, headY - space)) : std::max (y, std::max (ext, headY + space));
        }
        y += up ? -space * 1.2f : space * 1.2f;
        const float xa = egs[(size_t) i].x - space * 0.3f;
        const float xb = egs[(size_t) j].x + headWidth() + space * 0.3f;
        const float lw = std::max (1.0f, space * 0.12f);
        const float midX = (xa + xb) * 0.5f;
        const float hook = up ? space * 0.8f : -space * 0.8f;
        g.setColour (layerDimmed (l) ? theme.ink.withAlpha (0.42f) : theme.ink);
        g.drawLine (xa, y + hook, xa, y, lw);
        g.drawLine (xa, y, midX - space * 1.0f, y, lw);
        g.drawLine (midX + space * 1.0f, y, xb, y, lw);
        g.drawLine (xb, y, xb, y + hook, lw);
        glyph (g, (juce::juce_wchar) (Glyph::tuplet0 + juce::jlimit (0, 9, e.tuplet.actual)), midX - space * 0.45f, y + space * 0.75f, 0.9f);
    }
}

void StaffView::drawTiesAndSlurs (juce::Graphics& g)
{
    const float lw = std::max (1.0f, space * 0.12f);
    auto curve = [&] (float xa, float ya, float xb, float yb, bool above, float height) {
        juce::Path p;
        const float h = above ? -height : height;
        const float mx = (xa + xb) * 0.5f, my = (ya + yb) * 0.5f;
        p.startNewSubPath (xa, ya);
        p.quadraticTo (mx, my + h * 2.0f, xb, yb);
        p.quadraticTo (mx, my + h * 2.0f - (above ? -lw * 1.6f : lw * 1.6f), xa, ya);
        p.closeSubPath();
        g.fillPath (p);
    };

    for (int s = 0; s < (int) score->staves.size(); ++s)
        for (int l = 0; l < kLayers; ++l)
        {
            // flatten the layer's entries in written order
            struct Ref { int m, i; };
            std::vector<Ref> refs;
            for (int m = 0; m < (int) geo.measures.size(); ++m)
                for (int i = 0; i < (int) score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l].size(); ++i)
                    refs.push_back ({ m, i });
            auto entryAt = [&] (const Ref& r) -> const Entry& { return score->staves[(size_t) s].measures[(size_t) r.m].layers[(size_t) l][(size_t) r.i]; };
            auto geomAt = [&] (const Ref& r) -> const EntryGeom& { return geo.measures[(size_t) r.m].entries[(size_t) s][(size_t) l][(size_t) r.i]; };

            int slurFrom = -1;
            for (size_t r = 0; r < refs.size(); ++r)
            {
                const auto& e = entryAt (refs[r]);
                const auto& eg = geomAt (refs[r]);
                const Clef clef = score->clefAtEntry (s, refs[r].m, refs[r].i, l);
                g.setColour (entryColour (s, refs[r].m, l, refs[r].i, e));

                // ties to the next entry with the same pitch
                if (! e.rest && r + 1 < refs.size())
                {
                    const auto& n2 = entryAt (refs[r + 1]);
                    const auto& eg2 = geomAt (refs[r + 1]);
                    for (const auto& n : e.notes)
                    {
                        if (! n.tieToNext || n2.rest) continue;
                        bool match = false;
                        for (const auto& m2 : n2.notes) match = match || m2.pitch.midi() == n.pitch.midi();
                        if (! match) continue;
                        const int pos = staffPosition (n.pitch.dia, clef);
                        const bool above = ! eg.stemUp;
                        const float y = yForPos (s, pos) + (above ? -space * 0.5f : space * 0.5f);
                        curve (eg.x + headWidth() + space * 0.2f, y, eg2.x - space * 0.2f, y, above, space * 0.5f);
                    }
                }

                if (e.slurStart && ! e.rest) slurFrom = (int) r;
                if (e.slurEnd && slurFrom >= 0 && ! e.rest)
                {
                    const auto& a = refs[(size_t) slurFrom];
                    const auto& ea = entryAt (a);
                    const auto& ega = geomAt (a);
                    const bool above = ! ega.stemUp;
                    const Clef ca = score->clefAtEntry (s, a.m, a.i, l);
                    const auto& na = above ? ea.notes.back() : ea.notes.front();
                    const auto& nb = above ? e.notes.back() : e.notes.front();
                    const float yb = yForPos (s, staffPosition (nb.pitch.dia, clef)) + (above ? -space * 1.2f : space * 1.2f);
                    const float ya = yForPos (s, staffPosition (na.pitch.dia, ca)) + (above ? -space * 1.2f : space * 1.2f);
                    const float span = eg.x - ega.x;
                    curve (ega.x + headWidth() * 0.5f, ya, eg.x + headWidth() * 0.5f, yb, above,
                           juce::jlimit (space * 0.8f, space * 2.0f, span * 0.08f));
                    slurFrom = -1;
                }
            }
        }
}

float StaffView::xForTick (int m, Tick rel) const
{
    const auto& tx = geo.measures[(size_t) m].tickX;
    if (tx.empty()) return geo.measures[(size_t) m].firstNoteX;
    if (rel <= tx.front().first) return tx.front().second;
    for (size_t k = 0; k + 1 < tx.size(); ++k)
        if (rel >= tx[k].first && rel <= tx[k + 1].first)
        {
            const double f = (double) (rel - tx[k].first) / std::max<double> (1.0, (double) (tx[k + 1].first - tx[k].first));
            return tx[k].second + (float) f * (tx[k + 1].second - tx[k].second);
        }
    return tx.back().second;
}

int StaffView::measureAtX (float x) const
{
    for (int m = 0; m < (int) geo.measures.size(); ++m)
        if (x < geo.measures[(size_t) m].x + geo.measures[(size_t) m].width) return m;
    return (int) geo.measures.size() - 1;
}

double StaffView::tickAtX (float x) const
{
    if (geo.measures.empty()) return 0;
    const int m = measureAtX (x);
    const auto& tx = geo.measures[(size_t) m].tickX;
    double rel = 0;
    if (! tx.empty())
    {
        if (x <= tx.front().second) rel = (double) tx.front().first;
        else if (x >= tx.back().second) rel = (double) tx.back().first;
        else
            for (size_t k = 0; k + 1 < tx.size(); ++k)
                if (x >= tx[k].second && x <= tx[k + 1].second)
                {
                    const double f = (x - tx[k].second) / std::max (1.0f, tx[k + 1].second - tx[k].second);
                    rel = (double) tx[k].first + f * (double) (tx[k + 1].first - tx[k].first);
                    break;
                }
    }
    return (double) score->measureStart (m) + rel;
}

void StaffView::drawSelection (juce::Graphics& g)
{
    if (! editor || ! editor->selection().active || geo.measures.empty()) return;
    const auto& sel = editor->selection();
    const float top = mid (sel.staffFrom) - space * 3.0f;
    const float bottom = mid (std::min (sel.staffTo, (int) middleY.size() - 1)) + space * 3.0f;
    for (int m = 0; m < (int) geo.measures.size(); ++m)
    {
        const Tick ms = score->measureStart (m), cap = score->measureCapacity (m);
        if (ms + cap <= sel.start || ms >= sel.end) continue;
        const auto& mg = geo.measures[(size_t) m];
        const float xa = sel.start <= ms ? mg.x : xForTick (m, sel.start - ms) - space * 0.5f;
        const float xb = sel.end >= ms + cap ? mg.x + mg.width : xForTick (m, sel.end - ms) - space * 0.5f;
        g.setColour (theme.mark().withAlpha (0.13f));
        g.fillRect (juce::Rectangle<float> (xa, top, xb - xa, bottom - top));
    }
}

void StaffView::drawCursor (juce::Graphics& g)
{
    if (! editor || ! editor->frameActive()) return;
    const auto c = editor->cursor();
    if (c.measure >= (int) geo.measures.size()) return;
    const auto& mg = geo.measures[(size_t) c.measure];
    const auto& egs = mg.entries[(size_t) c.staff][(size_t) c.layer];
    const auto& es = score->staves[(size_t) c.staff].measures[(size_t) c.measure].layers[(size_t) c.layer];
    float x;
    if (c.entry < (int) egs.size())
        x = egs[(size_t) c.entry].x + headWidth() * 0.5f;
    else if (! egs.empty())
    {
        const Tick content = score->contentLength (c.staff, c.measure, c.layer);
        if (content < score->measureCapacity (c.measure) && c.layer > 0)
            x = xForTick (c.measure, content) + headWidth() * 0.5f;
        else
        {
            const double d = (double) std::max<Tick> (es.back().duration(), PPQ / 16);
            x = egs.back().x + headWidth() + space * (1.0f + 2.3f * (float) std::sqrt (d / (double) PPQ)) * 0.8f;
        }
        x = std::min (x, mg.x + mg.width - space * 1.0f);
    }
    else
        x = (c.layer > 0 ? mg.firstNoteX : mg.firstNoteX) + space * 1.0f;

    const float midY = mid (c.staff);
    const auto col = theme.mark();
    g.setColour (col.withAlpha (0.85f));
    g.fillRect (juce::Rectangle<float> (x - 1.0f, midY - space * 4.5f, 2.0f, space * 9.0f));
    const Clef clef = score->clefAtEntry (c.staff, c.measure, c.entry, c.layer);
    const int pos = staffPosition (c.dia, clef);
    const float y = yForPos (c.staff, pos);
    g.fillRect (juce::Rectangle<float> (x - space * 1.4f, y - 1.0f, space * 2.8f, 2.0f));
    // ledger hints for a crossbar outside the staff
    g.setColour (col.withAlpha (0.35f));
    for (int ll = 6; ll <= pos; ll += 2) g.fillRect (juce::Rectangle<float> (x - space * 1.0f, yForPos (c.staff, ll) - 0.5f, space * 2.0f, 1.0f));
    for (int ll = -6; ll >= pos; ll -= 2) g.fillRect (juce::Rectangle<float> (x - space * 1.0f, yForPos (c.staff, ll) - 0.5f, space * 2.0f, 1.0f));

    // mode text in the frame
    g.setColour (col);
    g.setFont (juce::FontOptions (std::max (10.0f, space * 1.1f), juce::Font::bold));
    juce::String mode;
    mode << "Layer " << (c.layer + 1);
    if (editor->insertMode()) mode << "  INSERT";
    if (editor->pendingTuplet().active())
        mode << "  " << editor->pendingTuplet().actual << ":" << editor->pendingTuplet().normal;
    g.drawText (mode, juce::Rectangle<float> (mg.x + space * 0.4f, mid ((int) middleY.size() - 1) + space * 4.2f, mg.width + space * 10, space * 1.6f),
                juce::Justification::centredLeft);
}

void StaffView::drawPlayhead (juce::Graphics& g)
{
    if (playheadTick < 0 || geo.measures.empty()) return;
    double t = playheadTick;
    for (int m = 0; m < (int) geo.measures.size(); ++m)
    {
        const Tick cap = score->measureCapacity (m);
        if (t < (double) cap || m + 1 == (int) geo.measures.size())
        {
            const float x = xForTick (m, (Tick) t);
            g.setColour (theme.playhead);
            g.fillRect (juce::Rectangle<float> (x, mid (0) - space * 4, 2.0f, mid ((int) middleY.size() - 1) - mid (0) + space * 8));
            return;
        }
        t -= (double) cap;
    }
}

// ---------------------------------------------------------------- velocity lane

void StaffView::drawLane (juce::Graphics& g)
{
    if (! showLane) return;
    const auto area = laneArea();
    g.setColour (theme.panel);
    g.fillRect (area);
    g.setColour (theme.line);
    g.fillRect (area.withHeight (1.0f));
    const auto inner = area.reduced (0, 10).withTrimmedTop (4);
    for (int k = 1; k <= 3; ++k)
    {
        const float y = inner.getBottom() - inner.getHeight() * (float) k / 4.0f;
        g.setColour (theme.line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (area.getX(), y, area.getWidth(), 1.0f));
    }
    g.setColour (theme.muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("VELOCITY", area.reduced (8, 4).removeFromTop (14), juce::Justification::topLeft);
    if (! score) return;

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    const auto col = theme.mark();
    for (int m = 0; m < (int) geo.measures.size(); ++m)
    {
        const auto& mg = geo.measures[(size_t) m];
        if (mg.x + mg.width - scrollX < 0 || mg.x - scrollX > (float) getWidth()) continue;
        g.setColour (theme.line.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (mg.x + mg.width - scrollX, area.getY(), 1.0f, area.getHeight()));
        for (int s = 0; s < (int) score->staves.size(); ++s)
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
                for (int i = 0; i < (int) es.size(); ++i)
                {
                    const auto& e = es[(size_t) i];
                    if (e.rest || e.notes.empty()) continue;
                    const int v = velocities[(size_t) s][(size_t) m][(size_t) l][(size_t) i];
                    const float x = mg.entries[(size_t) s][(size_t) l][(size_t) i].x + headWidth() * 0.5f - scrollX + (float) s * 2.0f;
                    const float y = inner.getBottom() - inner.getHeight() * (float) v / 127.0f;
                    const Tick t = score->measureStart (m) + score->onsetOf (s, m, l, i);
                    const bool selected = editor && editor->selection().contains (s, t);
                    const auto c = selected ? col : (e.velocity > 0 ? theme.ink.withAlpha (0.75f) : theme.muted.withAlpha (0.8f));
                    g.setColour (c.withAlpha (selected ? 0.9f : 0.55f));
                    g.fillRect (juce::Rectangle<float> (x - 1.0f, y, 2.0f, inner.getBottom() - y));
                    g.setColour (c);
                    g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
                    if (selected)
                    {
                        g.setColour (theme.paper);
                        g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
                    }
                }
            }
    }
    g.restoreState();
}

bool StaffView::laneHit (juce::Point<float> p, int& staff, int& measure, int& layer, int& entry) const
{
    float best = 9.0f;
    bool found = false;
    const float x = p.x + scrollX;
    for (int m = 0; m < (int) geo.measures.size(); ++m)
    {
        const auto& mg = geo.measures[(size_t) m];
        if (x < mg.x - 10 || x > mg.x + mg.width + 10) continue;
        for (int s = 0; s < (int) score->staves.size(); ++s)
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
                for (int i = 0; i < (int) es.size(); ++i)
                {
                    if (es[(size_t) i].rest) continue;
                    const float ex = mg.entries[(size_t) s][(size_t) l][(size_t) i].x + headWidth() * 0.5f + (float) s * 2.0f;
                    if (std::abs (ex - x) < best)
                    {
                        best = std::abs (ex - x);
                        staff = s; measure = m; layer = l; entry = i;
                        found = true;
                    }
                }
            }
    }
    return found;
}

// ---------------------------------------------------------------- input

std::vector<KeyStroke> StaffView::keyCandidates (const juce::KeyPress& kp)
{
    const auto mods = kp.getModifiers();
    unsigned base = 0;
    if (mods.isCommandDown()) base |= ModCtrl;
    if (mods.isAltDown()) base |= ModAlt;
    const unsigned withShift = base | (mods.isShiftDown() ? ModShift : 0u);
    const int code = kp.getKeyCode();
    const juce::juce_wchar ch = kp.getTextCharacter();
    std::vector<KeyStroke> out;

    struct Special { int juceKey; int key; };
    const Special specials[] = {
        { juce::KeyPress::upKey, KeyUp }, { juce::KeyPress::downKey, KeyDown },
        { juce::KeyPress::leftKey, KeyLeft }, { juce::KeyPress::rightKey, KeyRight },
        { juce::KeyPress::returnKey, KeyEnter }, { juce::KeyPress::backspaceKey, KeyBackspace },
        { juce::KeyPress::deleteKey, KeyDelete }, { juce::KeyPress::insertKey, KeyInsert },
        { juce::KeyPress::tabKey, KeyTab }, { juce::KeyPress::escapeKey, KeyEscape },
        { juce::KeyPress::homeKey, KeyHome }, { juce::KeyPress::endKey, KeyEnd },
        { juce::KeyPress::pageUpKey, KeyPageUp }, { juce::KeyPress::pageDownKey, KeyPageDown },
        { juce::KeyPress::spaceKey, KeySpace },
    };
    for (auto sp : specials)
        if (code == sp.juceKey)
            return { { sp.key, withShift } };

    // number pad
    if (code >= juce::KeyPress::numberPad0 && code <= juce::KeyPress::numberPad9)
        return { { '0' + (code - juce::KeyPress::numberPad0), withShift } };
    if (code == juce::KeyPress::numberPadAdd) return { { '+', base } };
    if (code == juce::KeyPress::numberPadSubtract) return { { '-', base } };
    if (code == juce::KeyPress::numberPadMultiply) return { { '*', base } };
    if (code == juce::KeyPress::numberPadDivide) return { { '/', base } };
    if (code == juce::KeyPress::numberPadDecimalPoint) return { { '.', base } };
    if (code == juce::KeyPress::numberPadEquals) return { { '=', base } };

    // Shifted number-row symbols on US and German layouts -> shift + digit.
    auto shiftedDigit = [] (juce::juce_wchar c) -> int {
        int i = juce::String (")!@#$%^&*(").indexOfChar (c);
        if (i >= 0) return '0' + i;
        i = juce::String (juce::CharPointer_UTF8 ("=!\"\xc2\xa7$%&/()")).indexOfChar (c);
        if (i >= 0) return '0' + i;
        return -1;
    };

    // With Command/Option held the text character is unreliable; use the key code.
    juce::juce_wchar c = (base != 0 || ch < 32) ? (juce::juce_wchar) code : ch;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';

    if (c >= 'a' && c <= 'z') return { { (int) c, withShift } };
    if (c >= '0' && c <= '9') return { { (int) c, withShift } };
    if (c == '=' && mods.isShiftDown() && base != 0) return { { '+', base }, { '=', withShift } };
    if (c > 32)
    {
        // a symbol as typed (layout independent), then the shift+digit reading
        out.push_back ({ (int) c, base });
        const int d = shiftedDigit (c);
        if (d >= 0) out.push_back ({ d, base | ModShift });
        if (out.size() == 1) out.push_back ({ (int) c, withShift });
        return out;
    }
    return {};
}

bool StaffView::keyPressed (const juce::KeyPress& kp)
{
    if (! editor || ! keyMap) return false;
    const auto candidates = keyCandidates (kp);
    if (candidates.empty()) return false;
    for (const auto& k : candidates)
    {
        const auto b = keyMap->lookup (k);
        if (b.command == Command::None) continue;
        if (onKey) onKey (describeKey (k), juce::String (commandName (b.command)) + (b.arg ? " " + juce::String (b.arg) : juce::String()));
        followCursor = true;
        switch (b.command)
        {
            case Command::TupletDialog: if (editor->frameActive()) showTupletDialog(); return true;
            case Command::DynamicsMenu: showDynamicsMenu(); return true;
            case Command::ShowCommands:
            case Command::PrevClip:
            case Command::NextClip: if (onWindowCommand) onWindowCommand (b.command); return true;
            case Command::MeasureMenu:
            {
                computeLayout();
                const auto& mg = geo.measures[(size_t) editor->cursor().measure];
                showMeasureMenu (editor->cursor().measure,
                                 juce::Rectangle<float> (mg.x - scrollX, 0, mg.width, (float) getHeight()).toNearestInt());
                return true;
            }
            default: break;
        }
        const bool wasFrame = editor->frameActive();
        const bool done = editor->perform (b);
        repaint();
        // with the frame closed, keys it does not use go to Bitwig (space, numbers...)
        if (! wasFrame && ! done) return false;
        return true; // bound keys are always consumed, even when nothing changed
    }
    if (onKey) onKey (describeKey (candidates.front()), {});
    return false; // unbound: let the host have it (e.g. space for play)
}

bool StaffView::hitTest (juce::Point<float> p, int& staff, int& measure, int& entry, int& dia) const
{
    if (! score || geo.measures.empty() || middleY.empty()) return false;
    const float x = p.x + scrollX, y = p.y + scrollY;
    staff = 0;
    float best = 1e9f;
    for (int s = 0; s < (int) middleY.size(); ++s)
        if (std::abs (y - mid (s)) < best) { best = std::abs (y - mid (s)); staff = s; }
    measure = -1;
    for (int m = 0; m < (int) geo.measures.size(); ++m)
        if (x >= geo.measures[(size_t) m].x && x < geo.measures[(size_t) m].x + geo.measures[(size_t) m].width) measure = m;
    if (measure < 0) return false;
    const int layer = editor ? editor->cursor().layer : 0;
    const auto& egs = geo.measures[(size_t) measure].entries[(size_t) staff][(size_t) layer];
    entry = (int) egs.size();
    for (int i = 0; i < (int) egs.size(); ++i)
        if (x < egs[(size_t) i].x + headWidth() + space * 0.6f) { entry = i; break; }
    const int pos = juce::roundToInt ((mid (staff) - y) / (space * 0.5f));
    dia = pos + clefMiddleLine (score->clefAtEntry (staff, measure, entry, layer));
    return true;
}

void StaffView::selectRange (juce::Point<float> a, juce::Point<float> b, bool extend)
{
    if (! editor || ! score || middleY.empty()) return;
    const float ya = std::min (a.y, b.y) + scrollY, yb = std::max (a.y, b.y) + scrollY;
    const float xa = std::min (a.x, b.x) + scrollX, xb = std::max (a.x, b.x) + scrollX;
    // staves the rectangle touches (or the nearest one)
    int s0 = -1, s1 = -1;
    for (int s = 0; s < (int) middleY.size(); ++s)
        if (yb >= mid (s) - space * 6 && ya <= mid (s) + space * 6) { if (s0 < 0) s0 = s; s1 = s; }
    if (s0 < 0)
    {
        float best = 1e9f;
        for (int s = 0; s < (int) middleY.size(); ++s)
            if (std::abs ((ya + yb) * 0.5f - mid (s)) < best) { best = std::abs ((ya + yb) * 0.5f - mid (s)); s0 = s1 = s; }
    }
    double t0 = tickAtX (xa), t1 = tickAtX (xb);
    const int m0 = score->measureAtTick ((Tick) t0), m1 = score->measureAtTick ((Tick) std::max (t0, t1 - 1));
    Selection sel;
    sel.active = true;
    sel.staffFrom = s0;
    sel.staffTo = s1;
    if (grid >= 10)
    {
        sel.start = score->measureStart (m0);
        sel.end = score->measureStart (m1) + score->measureCapacity (m1);
    }
    else if (grid > 0)
    {
        const Tick g0 = gridTicks (m0), ms0 = score->measureStart (m0), ms1 = score->measureStart (m1);
        sel.start = ms0 + ((Tick) t0 - ms0) / g0 * g0;
        sel.end = ms1 + (((Tick) std::ceil (t1) - ms1) + g0 - 1) / g0 * g0;
        if (sel.end <= sel.start) sel.end = sel.start + g0;
    }
    else
    {
        // free: exactly the entries the rectangle touches
        Tick lo = std::numeric_limits<Tick>::max(), hi = -1;
        for (int s = s0; s <= s1; ++s)
            for (int m = 0; m < (int) geo.measures.size(); ++m)
                for (int l = 0; l < kLayers; ++l)
                {
                    const auto& es = score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l];
                    const auto& egs = geo.measures[(size_t) m].entries[(size_t) s][(size_t) l];
                    Tick t = score->measureStart (m);
                    for (size_t i = 0; i < es.size(); ++i)
                    {
                        const float ex0 = egs[i].x - space * 0.3f, ex1 = egs[i].x + headWidth() + space * 0.3f;
                        if (ex1 >= xa && ex0 <= xb)
                        {
                            lo = std::min (lo, t);
                            hi = std::max (hi, t + std::max<Tick> (1, es[i].duration()));
                        }
                        t += es[i].duration();
                    }
                }
        if (hi < 0)
        {
            editor->clearSelection();
            editor->setSelection (editor->selection());
            return;
        }
        sel.start = lo;
        sel.end = hi;
    }
    if (extend && editor->selection().active)
    {
        const auto& old = editor->selection();
        sel.start = std::min (sel.start, old.start);
        sel.end = std::max (sel.end, old.end);
        sel.staffFrom = std::min (sel.staffFrom, old.staffFrom);
        sel.staffTo = std::max (sel.staffTo, old.staffTo);
    }
    editor->setSelection (sel);
}

void StaffView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (! score || ! editor) return;
    dragStart = dragNow = e.position;
    drag = Drag::None;

    // velocity lane: pick a note, drag to change it (all selected notes move together)
    if (showLane && laneArea().contains (e.position))
    {
        int s, m, l, i;
        if (laneHit (e.position, s, m, l, i))
        {
            const Tick t = score->measureStart (m) + score->onsetOf (s, m, l, i);
            if (! editor->selection().contains (s, t) || e.mods.isShiftDown())
            {
                Selection sel;
                if (e.mods.isShiftDown() && editor->selection().active) sel = editor->selection();
                const Tick d = std::max<Tick> (1, score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l][(size_t) i].duration());
                if (sel.active)
                {
                    sel.start = std::min (sel.start, t);
                    sel.end = std::max (sel.end, t + d);
                    sel.staffFrom = std::min (sel.staffFrom, s);
                    sel.staffTo = std::max (sel.staffTo, s);
                }
                else
                    sel = { true, s, s, t, t + d };
                editor->setSelection (sel);
            }
            // remember the selected notes and where their velocities started
            velDrag.clear();
            const auto& sel = editor->selection();
            for (int st = sel.staffFrom; st <= sel.staffTo; ++st)
                for (int mm = 0; mm < score->numMeasures(); ++mm)
                    for (int ll = 0; ll < kLayers; ++ll)
                    {
                        const auto& es = score->staves[(size_t) st].measures[(size_t) mm].layers[(size_t) ll];
                        Tick tt = score->measureStart (mm);
                        for (int ii = 0; ii < (int) es.size(); ++ii)
                        {
                            if (! es[(size_t) ii].rest && sel.contains (st, tt))
                                velDrag.push_back ({ st, mm, ll, ii, velocities[(size_t) st][(size_t) mm][(size_t) ll][(size_t) ii] });
                            tt += es[(size_t) ii].duration();
                        }
                    }
            editor->checkpoint();
            drag = Drag::Velocity;
        }
        else
        {
            editor->clearSelection();
            editor->setSelection (editor->selection());
        }
        repaint();
        return;
    }

    int staff, measure, entry, dia;
    if (! hitTest (e.position, staff, measure, entry, dia)) return;
    if (e.mods.isPopupMenu())
    {
        const auto& mg = geo.measures[(size_t) measure];
        showMeasureMenu (measure, juce::Rectangle<float> (mg.x - scrollX, 0, mg.width, (float) getHeight()).toNearestInt());
        return;
    }
    if (tool == Tool::Select)
    {
        selBeforeDrag = editor->selection();
        drag = Drag::Select;
        return;
    }
    followCursor = false;
    auto c = editor->cursor();
    editor->setCursor ({ staff, measure, entry, dia, c.layer });
    repaint();
}

void StaffView::mouseDrag (const juce::MouseEvent& e)
{
    dragNow = e.position;
    if (drag == Drag::Velocity)
    {
        const float h = laneArea().reduced (0, 10).getHeight();
        const int delta = juce::roundToInt ((dragStart.y - e.position.y) / std::max (1.0f, h) * 127.0f);
        for (const auto& v : velDrag)
            score->staves[(size_t) v.staff].measures[(size_t) v.measure].layers[(size_t) v.layer][(size_t) v.entry].velocity
                = juce::jlimit (1, 127, v.from + delta);
        if (onScoreEdited) onScoreEdited();
        repaint();
    }
    else if (drag == Drag::Select)
        repaint();
}

void StaffView::mouseUp (const juce::MouseEvent& e)
{
    if (drag == Drag::Select)
    {
        const bool click = e.getDistanceFromDragStart() < 4;
        if (click)
        {
            // a click selects the note under the mouse (grid "free"), or a grid cell / measure
            const auto p = e.position;
            selectRange (p.translated (-2, -2), p.translated (2, 2), e.mods.isShiftDown());
        }
        else
            selectRange (dragStart, e.position, e.mods.isShiftDown());
    }
    if (drag == Drag::Velocity)
        editor->edited();
    drag = Drag::None;
    velDrag.clear();
    repaint();
}

void StaffView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! editor || ! score) return;
    if (showLane && laneArea().contains (e.position))
    {
        // double-click a velocity: back to the dynamics' level
        int s, m, l, i;
        if (laneHit (e.position, s, m, l, i))
        {
            editor->checkpoint();
            score->staves[(size_t) s].measures[(size_t) m].layers[(size_t) l][(size_t) i].velocity = 0;
            editor->edited();
            repaint();
        }
        return;
    }
    int staff, measure, entry, dia;
    if (tool == Tool::Select && hitTest (e.position, staff, measure, entry, dia))
    {
        editor->setSelection ({ true, staff, staff, score->measureStart (measure), score->measureStart (measure) + score->measureCapacity (measure) });
        repaint();
    }
}

void StaffView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        setStaffSpace (space * (w.deltaY > 0 ? 1.1f : 0.9f));
        return;
    }
    followCursor = false;
    const bool vertical = std::abs (w.deltaY) > std::abs (w.deltaX) && ! e.mods.isShiftDown();
    const float visibleH = (float) getHeight() - laneHeight();
    if (vertical && geo.totalHeight > visibleH && e.position.y < visibleH)
    {
        scrollY = juce::jlimit (0.0f, std::max (0.0f, geo.totalHeight - visibleH), scrollY - w.deltaY * space * 30.0f);
    }
    else
    {
        const float d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * space * 40.0f;
        scrollX = juce::jlimit (0.0f, std::max (0.0f, geo.totalWidth - (float) getWidth()), scrollX - d);
    }
    repaint();
}

void StaffView::showTupletDialog()
{
    auto* w = new juce::AlertWindow ("Tuplet", "Notes : in the time of (for example 3:2, 5:4, 11:8)", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("ratio", "3:2");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<StaffView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result) {
        if (! safe) return;
        if (result == 1)
        {
            const auto text = w->getTextEditorContents ("ratio").retainCharacters ("0123456789:/ ");
            auto parts = juce::StringArray::fromTokens (text, ":/ ", "");
            parts.removeEmptyStrings();
            if (parts.size() == 2 && parts[0].getIntValue() > 0 && parts[1].getIntValue() > 0)
                safe->editor->startTuplet (parts[0].getIntValue(), parts[1].getIntValue());
        }
        safe->grabKeyboardFocus();
        safe->repaint();
    }), true);
}

void StaffView::showDynamicsMenu()
{
    if (! editor || ! score) return;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Dynamic (plays a fixed velocity)");
    for (int d = 0; d < (int) Dynamic::Count; ++d)
        menu.addItem (d + 1, juce::String (dynamicName ((Dynamic) d)) + "    velocity " + juce::String (score->playback.dynamicVelocity[(size_t) d]));
    juce::Component::SafePointer<StaffView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withParentComponent (getTopLevelComponent()).withMousePosition(),
                        [safe] (int r) {
                            if (! safe || r <= 0) return;
                            safe->editor->perform (Command::SetDynamic, r);
                            safe->grabKeyboardFocus();
                            safe->repaint();
                        });
}
// ---------------------------------------------------------------- measure menu

void StaffView::showMeasureMenu (int m, juce::Rectangle<int> area)
{
    if (! score || ! editor || m < 0 || m >= score->numMeasures()) return;
    juce::Component::SafePointer<StaffView> safe (this);
    const int staff = editor->cursor().staff;

    // Every action: snapshot for undo, change the score, notify.
    auto act = [safe] (std::function<void (Score&)> f) {
        return [safe, f] {
            if (! safe) return;
            safe->editor->checkpoint();
            f (*safe->score);
            safe->editor->edited();
            if (safe->onScoreEdited) safe->onScoreEdited();
            safe->repaint();
        };
    };

    juce::PopupMenu menu;
    menu.addSectionHeader ("Measure " + juce::String (m + 1));

    juce::PopupMenu time;
    const int common[][2] = { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 4 }, { 2, 2 }, { 3, 2 }, { 3, 8 }, { 5, 8 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
    const auto curTime = score->timeSigAt (m);
    for (auto& t : common)
    {
        const TimeSig ts { t[0], t[1] };
        time.addItem (juce::String (t[0]) + "/" + juce::String (t[1]), true, curTime == ts, act ([m, ts] (Score& s) {
            if (m == 0) s.timeSig = ts;
            else s.measures[(size_t) m].timeSig = ts;
        }));
    }
    time.addSeparator();
    for (int num = 1; num <= 15; ++num)
    {
        juce::PopupMenu sub;
        for (int den : { 1, 2, 4, 8, 16, 32 })
        {
            const TimeSig ts { num, den };
            sub.addItem (juce::String (num) + "/" + juce::String (den), true, curTime == ts, act ([m, ts] (Score& s) {
                if (m == 0) s.timeSig = ts;
                else s.measures[(size_t) m].timeSig = ts;
            }));
        }
        time.addSubMenu ("Other " + juce::String (num) + "/...", sub);
    }
    if (m > 0 && score->measures[(size_t) m].timeSig)
        time.addItem ("Remove meter change", act ([m] (Score& s) { s.measures[(size_t) m].timeSig.reset(); }));
    menu.addSubMenu ("Time signature", time);

    juce::PopupMenu key;
    for (int k = -7; k <= 7; ++k)
        key.addItem (kKeyNames[k + 7], true, score->keyAt (m) == k, act ([m, k] (Score& s) {
            if (m == 0) s.keyFifths = k;
            else s.measures[(size_t) m].keyFifths = k;
        }));
    if (m > 0 && score->measures[(size_t) m].keyFifths)
        key.addItem ("Remove key change", act ([m] (Score& s) { s.measures[(size_t) m].keyFifths.reset(); }));
    menu.addSubMenu ("Key signature", key);

    juce::PopupMenu clefStart, clefHere;
    for (int c = 0; c < (int) Clef::Count; ++c)
    {
        const Clef cl = (Clef) c;
        clefStart.addItem (clefName (cl), true, score->clefAt (staff, m) == cl, act ([m, staff, cl] (Score& s) {
            if (m == 0) s.staves[(size_t) staff].clef = cl;
            else s.staves[(size_t) staff].measures[(size_t) m].clefChange = cl;
        }));
    }
    const int entry = editor->cursor().measure == m ? editor->cursor().entry : 0;
    const int layer = editor->cursor().layer;
    const bool hasEntry = entry < (int) score->staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer].size();
    for (int c = 0; c < (int) Clef::Count; ++c)
    {
        const Clef cl = (Clef) c;
        clefHere.addItem (clefName (cl), hasEntry, false, act ([m, staff, entry, layer, cl] (Score& s) {
            s.staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer][(size_t) entry].clefBefore = cl;
        }));
    }
    if (hasEntry && score->staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer][(size_t) entry].clefBefore)
        clefHere.addItem ("Remove clef change here", act ([m, staff, entry, layer] (Score& s) {
            s.staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer][(size_t) entry].clefBefore.reset();
        }));
    if (m > 0 && score->staves[(size_t) staff].measures[(size_t) m].clefChange)
        clefStart.addItem ("Remove clef change", act ([m, staff] (Score& s) { s.staves[(size_t) staff].measures[(size_t) m].clefChange.reset(); }));
    menu.addSubMenu ("Clef at start of measure", clefStart);
    menu.addSubMenu ("Clef before the note at the cursor", clefHere, hasEntry);

    juce::PopupMenu dyn;
    for (int d = 0; d < (int) Dynamic::Count; ++d)
    {
        const bool on = hasEntry && score->staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer][(size_t) entry].dynamic == (Dynamic) d;
        dyn.addItem (dynamicName ((Dynamic) d), hasEntry, on, act ([m, staff, entry, layer, d, on] (Score& s) {
            auto& e = s.staves[(size_t) staff].measures[(size_t) m].layers[(size_t) layer][(size_t) entry];
            if (on) e.dynamic.reset(); else e.dynamic = (Dynamic) d;
        }));
    }
    menu.addSubMenu ("Dynamic at the cursor", dyn, hasEntry);

    juce::PopupMenu bars;
    for (int b = 0; b < (int) Barline::Count; ++b)
        bars.addItem (barlineName ((Barline) b), true, score->measures[(size_t) m].barline == (Barline) b,
                      act ([m, b] (Score& s) { s.measures[(size_t) m].barline = (Barline) b; }));
    menu.addSubMenu ("Barline at end", bars);

    juce::PopupMenu rep;
    const auto& info = score->measures[(size_t) m];
    rep.addItem ("Start repeat", true, info.repeatStart, act ([m] (Score& s) { s.measures[(size_t) m].repeatStart = ! s.measures[(size_t) m].repeatStart; }));
    rep.addItem ("End repeat", true, info.repeatEnd, act ([m] (Score& s) { s.measures[(size_t) m].repeatEnd = ! s.measures[(size_t) m].repeatEnd; }));
    juce::PopupMenu times;
    for (int t = 2; t <= 8; ++t)
        times.addItem ("Play " + juce::String (t) + " times", info.repeatEnd, info.repeatTimes == t,
                       act ([m, t] (Score& s) { s.measures[(size_t) m].repeatTimes = t; }));
    rep.addSubMenu ("End repeat plays", times, info.repeatEnd);
    juce::PopupMenu endings;
    const std::pair<const char*, unsigned> endingChoices[] = { { "None", 0u }, { "1st ending", 1u }, { "2nd ending", 2u }, { "3rd ending", 4u }, { "1st and 2nd", 3u } };
    for (auto [label, mask] : endingChoices)
        endings.addItem (label, true, info.endingMask == mask, act ([m, mask = mask] (Score& s) { s.measures[(size_t) m].endingMask = mask; }));
    rep.addSubMenu ("Ending", endings);
    menu.addSubMenu ("Repeats and endings", rep);

    if (m == 0)
    {
        const Tick len = score->contentLength (staff, 0);
        const bool pickup = info.pickupLength > 0;
        menu.addItem (pickup ? "Make full measure (remove pickup)" : "Make pickup measure from its notes", pickup || (len > 0 && len < score->timeSigAt (0).measureLength()), pickup,
                      act ([len, pickup] (Score& s) { s.measures[0].pickupLength = pickup ? 0 : len; }));
    }

    menu.addSeparator();
    menu.addItem ("Insert measure before", act ([m] (Score& s) { s.insertMeasure (m); }));
    menu.addItem ("Add measure after", act ([m] (Score& s) { s.insertMeasure (m + 1); }));
    menu.addItem ("Delete measure", score->numMeasures() > 1, false, act ([m] (Score& s) { s.removeMeasure (m); }));
    menu.addSeparator();
    menu.addItem ("Grand staff", true, score->isGrandStaff(), act ([] (Score& s) { s.setGrandStaff (! s.isGrandStaff()); }));
    juce::PopupMenu chan;
    for (int c = 1; c <= 16; ++c)
        chan.addItem ("Channel " + juce::String (c), true, score->staves[(size_t) staff].midiChannel == c,
                      act ([staff, c] (Score& s) { s.staves[(size_t) staff].midiChannel = c; }));
    menu.addSubMenu ("MIDI channel for this staff", chan);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (area))
                            .withParentComponent (getTopLevelComponent()));
}
