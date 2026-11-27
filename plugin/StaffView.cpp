#include "StaffView.h"

#include "Glyphs.h"
#include "staffpaper/Layout.h"

#include <cmath>
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

StaffView::Theme StaffView::Theme::light()
{
    return { juce::Colour (0xfffbfaf6), juce::Colour (0xff151515), juce::Colour (0xffb0b0b0),
             juce::Colour (0xff2f6fd6), juce::Colour (0xffd23c3c), juce::Colour (0xff19a974) };
}

StaffView::Theme StaffView::Theme::dark()
{
    return { juce::Colour (0xff1c1d20), juce::Colour (0xffe6e3dc), juce::Colour (0xff5f6166),
             juce::Colour (0xff6ea0ff), juce::Colour (0xffff6b6b), juce::Colour (0xff3ddc97) };
}

void StaffView::setDark (bool d)
{
    theme = d ? Theme::dark() : Theme::light();
    repaint();
}

void StaffView::setScore (Score* s, Editor* e)
{
    score = s;
    editor = e;
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
    repaint();
}

int StaffView::preferredHeight() const
{
    const int staves = score ? (int) score->staves.size() : 1;
    return (int) (space * (16.0f + 13.0f * (float) (staves - 1)));
}

// ---------------------------------------------------------------- layout

StaffView::MeasureGeom StaffView::layoutMeasure (int m, float x, bool sysStart, float stretch)
{
    const int nStaves = (int) score->staves.size();
    const Cursor cur = editor ? editor->cursor() : Cursor {};
    {
        MeasureGeom mg;
        mg.x = x;
        mg.systemStart = sysStart;
        mg.frame = (editor != nullptr && m == cur.measure);
        const float scale = (mg.frame ? 1.7f : 1.0f) * stretch;
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

        // ---- columns across staves
        struct Key
        {
            Tick tick;
            int order;
            bool operator< (const Key& o) const { return tick != o.tick ? tick < o.tick : order < o.order; }
        };
        std::map<Key, float> pre, post, spacing;
        std::vector<std::vector<Key>> entryKeys ((size_t) nStaves);
        const Tick cap = score->measureCapacity (m);

        for (int s = 0; s < nStaves; ++s)
        {
            const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
            const auto onsets = entryOnsets (sm);
            const int n = (int) sm.entries.size();
            for (int i = 0; i < n; ++i)
            {
                const auto& e = sm.entries[(size_t) i];
                int order = 0;
                if (e.grace)
                {
                    int remaining = 0;
                    for (int j = i; j < n && sm.entries[(size_t) j].grace; ++j) ++remaining;
                    order = -remaining;
                }
                Key k { onsets[(size_t) i], order };
                entryKeys[(size_t) s].push_back (k);
                float pr = 0;
                for (int ni = 0; ni < (int) e.notes.size(); ++ni)
                    if (score->displayedAccidental (s, m, i, ni)) pr = std::max (pr, space * (e.notes[(size_t) ni].parentheses ? 2.2f : 1.4f));
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
            mg.tickX.push_back ({ k.tick, cx });
            cx += w + post[k];
        }
        if (spacing.empty()) cx += space * 7.0f * scale;
        cx += space * 0.6f;
        if (info.repeatEnd) cx += space * 1.4f;
        mg.width = std::max (cx - x, space * 6.0f);
        mg.tickX.push_back ({ cap, x + mg.width });

        mg.entries.resize ((size_t) nStaves);
        for (int s = 0; s < nStaves; ++s)
        {
            const auto& keys = entryKeys[(size_t) s];
            for (size_t i = 0; i < keys.size(); ++i)
            {
                EntryGeom eg;
                eg.x = colX[keys[i]];
                mg.entries[(size_t) s].push_back (eg);
            }
        }
        return mg;
    }
}

void StaffView::computeLayout()
{
    geo = {};
    if (! score) return;
    const int nStaves = (int) score->staves.size();
    const float staffGap = 13.0f;
    for (int s = 0; s < nStaves; ++s)
        geo.middleY.push_back (space * (8.0f + staffGap * (float) s));
    const float left = space * (nStaves > 1 ? 3.0f : 1.5f);
    const float systemHeight = space * (staffGap * (float) nStaves + 4.0f);

    if (! pageView)
    {
        float x = left;
        for (int m = 0; m < score->numMeasures(); ++m)
        {
            auto mg = layoutMeasure (m, x, m == 0, 1.0f);
            x += mg.width;
            geo.measures.push_back (std::move (mg));
        }
        geo.systems.push_back ({ 0, score->numMeasures() - 1, 0.0f, left, x });
        geo.totalWidth = x + space * 4.0f;
        geo.totalHeight = systemHeight;
    }
    else
    {
        const float right = std::max (space * 30.0f, (float) getWidth() - space * 2.0f);
        const float avail = right - left;
        int m = 0;
        float y = 0;
        while (m < score->numMeasures())
        {
            // fill a system greedily with natural widths
            std::vector<float> widths;
            float sum = 0;
            int first = m;
            while (m < score->numMeasures())
            {
                const float w = layoutMeasure (m, 0, m == first, 1.0f).width;
                if (! widths.empty() && sum + w > avail) break;
                widths.push_back (w);
                sum += w;
                ++m;
            }
            const bool lastSystem = (m >= score->numMeasures());
            float stretch = (lastSystem && sum < avail * 0.7f) ? 1.0f : std::max (1.0f, avail / std::max (1.0f, sum));
            // stretching only scales note spacing, so iterate once to land close to the edge
            float x = left;
            std::vector<MeasureGeom> row;
            for (int k = first; k < m; ++k)
            {
                auto mg = layoutMeasure (k, x, k == first, stretch);
                x += mg.width;
                row.push_back (std::move (mg));
            }
            if (! lastSystem || stretch > 1.0f)
            {
                // absorb the remainder in the last measure
                auto& lastM = row.back();
                const float extra = right - (lastM.x + lastM.width);
                lastM.width += extra;
                lastM.tickX.back().second += extra;
                x = right;
            }
            for (auto& mg : row)
            {
                mg.yOff = y;
                geo.measures.push_back (std::move (mg));
            }
            geo.systems.push_back ({ first, m - 1, y, left, x });
            y += systemHeight;
        }
        geo.totalWidth = (float) getWidth();
        geo.totalHeight = y;
    }

    for (int m = 0; m < (int) geo.measures.size(); ++m)
    {
        yOff = geo.measures[(size_t) m].yOff;
        for (int s = 0; s < nStaves; ++s)
            computeStems (s, m);
    }
    yOff = 0;
}

void StaffView::computeStems (int s, int m)
{
    auto& mg = geo.measures[(size_t) m];
    const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
    const float hw = headWidth();

    auto noteY = [&] (const Entry& e, int idx, int entryIndex) {
        const Clef c = score->clefAtEntry (s, m, entryIndex);
        return yForPos (s, staffPosition (e.notes[(size_t) idx].pitch.dia, c));
    };

    for (int i = 0; i < (int) sm.entries.size(); ++i)
    {
        const auto& e = sm.entries[(size_t) i];
        auto& eg = mg.entries[(size_t) s][(size_t) i];
        if (e.rest || e.notes.empty()) continue;
        const float scale = e.grace ? 0.65f : 1.0f;
        eg.stemUp = autoStemUp (e, score->clefAtEntry (s, m, i));
        eg.stemX = eg.stemUp ? eg.x + hw * scale - space * 0.06f : eg.x + space * 0.06f;
        const float top = noteY (e, (int) e.notes.size() - 1, i);
        const float bot = noteY (e, 0, i);
        const float len = space * 3.5f * scale;
        eg.stemEndY = eg.stemUp ? top - len : bot + len;
        // stems reach the middle line when notes are far away
        const float midY = mid (s);
        if (eg.stemUp && eg.stemEndY > midY && ! e.grace) eg.stemEndY = midY;
        if (! eg.stemUp && eg.stemEndY < midY && ! e.grace) eg.stemEndY = midY;
    }

    for (const auto& group : beamGroups (*score, s, m))
    {
        const bool up = groupStemUp (sm, group, score->clefAtEntry (s, m, group.front()));
        const int maxBeams = [&] { int b = 1; for (int i : group) b = std::max (b, beamCount (sm.entries[(size_t) i].value)); return b; }();
        const float minLen = space * (2.5f + 0.75f * (float) (maxBeams - 1));
        std::vector<float> ext; // furthest notehead in stem direction
        bool flat = false;
        for (int i : group)
        {
            const auto& e = sm.entries[(size_t) i];
            auto& eg = mg.entries[(size_t) s][(size_t) i];
            eg.stemUp = up;
            eg.beamed = true;
            eg.stemX = up ? eg.x + hw - space * 0.06f : eg.x + space * 0.06f;
            ext.push_back (up ? noteY (e, (int) e.notes.size() - 1, i) : noteY (e, 0, i));
            flat = flat || e.flatBeam;
        }
        const auto& first = mg.entries[(size_t) s][(size_t) group.front()];
        const auto& last = mg.entries[(size_t) s][(size_t) group.back()];
        const float dx = std::max (1.0f, last.stemX - first.stemX);
        float slope = flat ? 0.0f : (ext.back() - ext.front()) / dx;
        slope = juce::jlimit (-space * 1.0f / dx, space * 1.0f / dx, slope);
        // place the beam so every stem is at least minLen long
        float offset = up ? 1e9f : -1e9f;
        for (size_t k = 0; k < group.size(); ++k)
        {
            const float sx = mg.entries[(size_t) s][(size_t) group[k]].stemX;
            const float lineY = slope * (sx - first.stemX);
            const float need = up ? ext[k] - minLen - lineY : ext[k] + minLen - lineY;
            offset = up ? std::min (offset, need) : std::max (offset, need);
        }
        for (int i : group)
        {
            auto& eg = mg.entries[(size_t) s][(size_t) i];
            eg.stemEndY = offset + slope * (eg.stemX - first.stemX);
        }
    }
}

void StaffView::ensureCursorVisible()
{
    if (! editor || geo.measures.empty()) return;
    const auto& mg = geo.measures[(size_t) juce::jlimit (0, (int) geo.measures.size() - 1, editor->cursor().measure)];
    if (pageView)
    {
        scrollX = 0;
        const float h = (float) getHeight();
        const float sysH = geo.systems.empty() ? h : (geo.totalHeight / (float) geo.systems.size());
        if (mg.yOff - scrollY < 0) scrollY = std::max (0.0f, mg.yOff);
        else if (mg.yOff + sysH - scrollY > h) scrollY = std::max (0.0f, mg.yOff + sysH - h);
        return;
    }
    scrollY = 0;
    const float w = (float) getWidth();
    if (mg.x - scrollX < space * 2)
        scrollX = std::max (0.0f, mg.x - space * 4);
    else if (mg.x + mg.width - scrollX > w - space * 2)
        scrollX = std::max (0.0f, std::min (mg.x - space * 4, mg.x + mg.width - w + space * 6));
}

void StaffView::setPageView (bool page)
{
    pageView = page;
    scrollX = scrollY = 0;
    followCursor = true;
    repaint();
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

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (-scrollX, -scrollY));
    const int last = (int) geo.middleY.size() - 1;

    // frame behind everything
    if (editor)
    {
        const int cm = editor->cursor().measure;
        useMeasure (cm);
        const auto& mg = geo.measures[(size_t) cm];
        const float top = mid (0) - space * 6.0f;
        const float bottom = mid (last) + space * 6.0f;
        g.setColour (theme.frame.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (mg.x, top, mg.width, bottom - top));
        g.setColour (theme.frame);
        g.drawRect (juce::Rectangle<float> (mg.x, top, mg.width, bottom - top), 1.5f);
    }

    const juce::Rectangle<float> visible (scrollX - space * 20, scrollY - space * 20, (float) getWidth() + space * 40, (float) getHeight() + space * 40);
    for (const auto& sys : geo.systems)
    {
        yOff = sys.yOff;
        if (mid (last) + space * 8 < visible.getY() || mid (0) - space * 8 > visible.getBottom()) continue;
        g.setColour (theme.ink);
        drawStaffLines (g, sys.x0, sys.x1);
        // system start: brace and connecting line
        const float top = mid (0) - space * 2.0f;
        const float bottom = mid (last) + space * 2.0f;
        g.fillRect (juce::Rectangle<float> (sys.x0 - space * 0.08f, top, space * 0.16f, bottom - top));
        if (score->staves.size() > 1)
        {
            // the brace glyph is one staff tall: stretch it vertically to span the system
            const float k = (bottom - top) / (space * 4.0f);
            const float bx = sys.x0 - space * 1.3f;
            g.saveState();
            g.addTransform (juce::AffineTransform::scale (0.6f + 0.4f * k, k, bx + space, bottom));
            glyph (g, Glyph::brace, bx, bottom);
            g.restoreState();
        }

        for (int m = sys.first; m <= sys.last; ++m)
        {
            const auto& mg = geo.measures[(size_t) m];
            if (mg.x + mg.width < visible.getX() || mg.x > visible.getRight()) continue;
            drawMeasure (g, m);
        }
    }
    yOff = 0;
    drawTiesAndSlurs (g);
    drawCursor (g);
    drawPlayhead (g);
    yOff = 0;
    g.restoreState();
}

void StaffView::drawStaffLines (juce::Graphics& g, float x0, float x1)
{
    const float t = std::max (1.0f, space * 0.11f);
    for (size_t s = 0; s < geo.middleY.size(); ++s)
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
    const float bottom = mid ((int) geo.middleY.size() - 1) + space * 2.0f;
    const float thin = std::max (1.0f, space * 0.16f), thick = space * 0.5f;

    auto line = [&] (float lx, float w) { g.fillRect (juce::Rectangle<float> (lx - w, top, w, bottom - top)); };
    auto dots = [&] (float dx) {
        for (size_t s = 0; s < geo.middleY.size(); ++s)
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
    g.setColour (theme.ink.withAlpha (0.55f));
    g.setFont (juce::FontOptions (space * 1.2f));
    g.drawText (juce::String (m + 1), juce::Rectangle<float> (mg.x + 2, mid (0) - space * 5.0f, space * 4, space * 1.5f),
                juce::Justification::centredLeft);
    g.setColour (theme.ink);

    // header
    useMeasure (m);
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
        const float top = mid (0) - space * 2.0f, bottom = mid ((int) geo.middleY.size() - 1) + space * 2.0f;
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
    {
        for (int i = 0; i < (int) score->staves[(size_t) s].measures[(size_t) m].entries.size(); ++i)
            drawEntry (g, s, m, i);
        drawBeams (g, s, m);
        drawTuplets (g, s, m);
    }

    // overfull measure warning
    for (int s = 0; s < nStaves; ++s)
        if (score->contentLength (s, m) > score->measureCapacity (m))
        {
            g.setColour (theme.cursor.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (mg.x, mid (s) - space * 2, mg.width, space * 4));
            g.setColour (theme.ink);
        }

    drawBarline (g, m, mg.x + mg.width);
}

void StaffView::drawEntry (juce::Graphics& g, int s, int m, int i)
{
    const auto& e = score->staves[(size_t) s].measures[(size_t) m].entries[(size_t) i];
    const auto& eg = geo.measures[(size_t) m].entries[(size_t) s][(size_t) i];
    const Clef clef = score->clefAtEntry (s, m, i);
    const float sc = e.grace ? 0.65f : 1.0f;
    const float hw = headWidth() * sc;
    g.setColour (e.hidden ? theme.hidden : theme.ink);

    if (e.clefBefore)
    {
        // pre-space includes the clef; draw it just left of any accidentals
        float clefX = eg.x - space * 3.0f;
        for (int ni = 0; ni < (int) e.notes.size(); ++ni)
            if (score->displayedAccidental (s, m, i, ni)) { clefX -= space * 1.4f; break; }
        drawClef (g, *e.clefBefore, s, clefX, true);
    }

    if (e.rest)
    {
        int pos = 0;
        if (e.value == 7) pos = 2;
        const auto& es = score->staves[(size_t) s].measures[(size_t) m].entries;
        const bool wholeMeasureRest = es.size() == 1 && e.value == 7;
        float rx = eg.x;
        if (wholeMeasureRest)
        {
            const auto& mg = geo.measures[(size_t) m];
            rx = (mg.firstNoteX + mg.x + mg.width) * 0.5f - hw * 0.5f;
        }
        glyph (g, restGlyph (e.value), rx, yForPos (s, pos), sc);
        for (int d = 0; d < e.dots; ++d)
            g.fillEllipse (rx + space * (1.6f + 0.6f * (float) d), yForPos (s, 1) - space * 0.2f, space * 0.4f, space * 0.4f);
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
        for (int l = 6; l <= pos; l += 2)
            g.fillRect (juce::Rectangle<float> (headX[(size_t) k] - space * 0.4f, yForPos (s, l) - lw * 0.5f, hw + space * 0.8f, lw));
        for (int l = -6; l >= pos; l -= 2)
            g.fillRect (juce::Rectangle<float> (headX[(size_t) k] - space * 0.4f, yForPos (s, l) - lw * 0.5f, hw + space * 0.8f, lw));

        if (auto acc = score->displayedAccidental (s, m, i, k))
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
        if (e.grace && e.value <= 4)
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

void StaffView::drawBeams (juce::Graphics& g, int s, int m)
{
    const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
    const auto& egs = geo.measures[(size_t) m].entries[(size_t) s];
    for (const auto& group : beamGroups (*score, s, m))
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
            g.setColour (sm.entries[(size_t) group[a]].hidden ? theme.hidden : theme.ink);
            g.fillPath (p);
        };
        beamAt (0, group.size() - 1, 0);
        for (int level = 1; level < 4; ++level)
        {
            for (size_t k = 0; k < group.size(); ++k)
            {
                const bool here = beamCount (sm.entries[(size_t) group[k]].value) > level;
                if (! here) continue;
                const bool next = k + 1 < group.size() && beamCount (sm.entries[(size_t) group[k + 1]].value) > level;
                const bool prev = k > 0 && beamCount (sm.entries[(size_t) group[k - 1]].value) > level;
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

void StaffView::drawTuplets (juce::Graphics& g, int s, int m)
{
    const auto& sm = score->staves[(size_t) s].measures[(size_t) m];
    const auto& egs = geo.measures[(size_t) m].entries[(size_t) s];
    for (int i = 0; i < (int) sm.entries.size(); ++i)
    {
        const auto& e = sm.entries[(size_t) i];
        if (! e.tupletStart || ! e.tuplet.active()) continue;
        int j = i;
        while (j + 1 < (int) sm.entries.size() && sm.entries[(size_t) j + 1].tuplet.active() && ! sm.entries[(size_t) j + 1].tupletStart
               && j - i + 1 < e.tuplet.actual)
            ++j;
        const bool up = egs[(size_t) i].stemUp;
        float y = up ? 1e9f : -1e9f;
        for (int k = i; k <= j; ++k)
        {
            const auto& eg = egs[(size_t) k];
            const auto& ek = sm.entries[(size_t) k];
            float ext = eg.stemEndY;
            if (ek.rest || ek.value >= 7) ext = mid (s) + (up ? -space * 3 : space * 3);
            y = up ? std::min (y, ext) : std::max (y, ext);
        }
        y += up ? -space * 1.4f : space * 1.4f;
        const float xa = egs[(size_t) i].x - space * 0.3f;
        const float xb = egs[(size_t) j].x + headWidth() + space * 0.3f;
        const float lw = std::max (1.0f, space * 0.12f);
        const float mid = (xa + xb) * 0.5f;
        const float hook = up ? space * 0.8f : -space * 0.8f;
        g.setColour (theme.ink);
        g.drawLine (xa, y + hook, xa, y, lw);
        g.drawLine (xa, y, mid - space * 1.0f, y, lw);
        g.drawLine (mid + space * 1.0f, y, xb, y, lw);
        g.drawLine (xb, y, xb, y + hook, lw);
        glyph (g, (juce::juce_wchar) (Glyph::tuplet0 + juce::jlimit (0, 9, e.tuplet.actual)), mid - space * 0.45f, y + space * 0.75f, 0.9f);
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
    {
        // flatten the staff's entries in written order
        struct Ref { int m, i; };
        std::vector<Ref> refs;
        for (int m = 0; m < (int) geo.measures.size(); ++m)
            for (int i = 0; i < (int) score->staves[(size_t) s].measures[(size_t) m].entries.size(); ++i)
                refs.push_back ({ m, i });

        int slurFrom = -1;
        for (size_t r = 0; r < refs.size(); ++r)
        {
            const auto& e = score->staves[(size_t) s].measures[(size_t) refs[r].m].entries[(size_t) refs[r].i];
            const auto& eg = geo.measures[(size_t) refs[r].m].entries[(size_t) s][(size_t) refs[r].i];
            const Clef clef = score->clefAtEntry (s, refs[r].m, refs[r].i);
            useMeasure (refs[r].m);
            g.setColour (e.hidden ? theme.hidden : theme.ink);

            // ties to the next entry with the same pitch
            if (! e.rest && r + 1 < refs.size())
            {
                const auto& n2 = score->staves[(size_t) s].measures[(size_t) refs[r + 1].m].entries[(size_t) refs[r + 1].i];
                const auto& eg2 = geo.measures[(size_t) refs[r + 1].m].entries[(size_t) s][(size_t) refs[r + 1].i];
                for (const auto& n : e.notes)
                {
                    if (! n.tieToNext) continue;
                    if (n2.rest) continue;
                    bool match = false;
                    for (const auto& m2 : n2.notes) match = match || m2.pitch.midi() == n.pitch.midi();
                    if (! match) continue;
                    const int pos = staffPosition (n.pitch.dia, clef);
                    const bool above = ! eg.stemUp;
                    const float y = yForPos (s, pos) + (above ? -space * 0.5f : space * 0.5f);
                    const bool sameSystem = geo.measures[(size_t) refs[r + 1].m].yOff == geo.measures[(size_t) refs[r].m].yOff;
                    const float endX = sameSystem ? eg2.x - space * 0.2f : eg.x + headWidth() + space * 3.0f;
                    curve (eg.x + headWidth() + space * 0.2f, y, endX, y, above, space * 0.5f);
                }
            }

            if (e.slurStart && ! e.rest) slurFrom = (int) r;
            if (e.slurEnd && slurFrom >= 0 && ! e.rest)
            {
                const auto& a = refs[(size_t) slurFrom];
                const auto& ea = score->staves[(size_t) s].measures[(size_t) a.m].entries[(size_t) a.i];
                const auto& ega = geo.measures[(size_t) a.m].entries[(size_t) s][(size_t) a.i];
                const bool above = ! ega.stemUp;
                const Clef ca = score->clefAtEntry (s, a.m, a.i);
                const auto& na = above ? ea.notes.back() : ea.notes.front();
                const auto& nb = above ? e.notes.back() : e.notes.front();
                const float yb = yForPos (s, staffPosition (nb.pitch.dia, clef)) + (above ? -space * 1.2f : space * 1.2f);
                useMeasure (a.m);
                const float ya = yForPos (s, staffPosition (na.pitch.dia, ca)) + (above ? -space * 1.2f : space * 1.2f);
                if (geo.measures[(size_t) a.m].yOff == geo.measures[(size_t) refs[r].m].yOff)
                {
                    const float span = eg.x - ega.x;
                    curve (ega.x + headWidth() * 0.5f, ya, eg.x + headWidth() * 0.5f, yb, above,
                           juce::jlimit (space * 0.8f, space * 2.0f, span * 0.08f));
                }
                else
                {
                    // slur across a system break: draw both halves
                    float sysEnd = ega.x + space * 6;
                    float sysStart = eg.x - space * 6;
                    for (const auto& sys : geo.systems)
                    {
                        if (a.m >= sys.first && a.m <= sys.last) sysEnd = sys.x1;
                        if (refs[r].m >= sys.first && refs[r].m <= sys.last) sysStart = geo.measures[(size_t) sys.first].firstNoteX - space;
                    }
                    curve (ega.x + headWidth() * 0.5f, ya, sysEnd, ya, above, space * 1.2f);
                    useMeasure (refs[r].m);
                    curve (sysStart, yb, eg.x + headWidth() * 0.5f, yb, above, space * 1.2f);
                }
                slurFrom = -1;
            }
        }
    }
}

void StaffView::drawCursor (juce::Graphics& g)
{
    if (! editor) return;
    const auto c = editor->cursor();
    if (c.measure >= (int) geo.measures.size()) return;
    useMeasure (c.measure);
    const auto& mg = geo.measures[(size_t) c.measure];
    const auto& egs = mg.entries[(size_t) c.staff];
    float x;
    if (c.entry < (int) egs.size())
        x = egs[(size_t) c.entry].x + headWidth() * 0.5f;
    else if (! egs.empty())
    {
        const auto& lastE = score->staves[(size_t) c.staff].measures[(size_t) c.measure].entries.back();
        const double d = (double) std::max<Tick> (lastE.duration(), PPQ / 16);
        x = egs.back().x + headWidth() + space * (1.0f + 2.3f * (float) std::sqrt (d / (double) PPQ)) * 0.8f;
        x = std::min (x, mg.x + mg.width - space * 1.0f);
    }
    else
        x = mg.firstNoteX + space * 1.0f;

    const float midY = mid (c.staff);
    // insertion bar
    g.setColour (theme.cursor.withAlpha (0.8f));
    g.fillRect (juce::Rectangle<float> (x - 1.0f, midY - space * 4.5f, 2.0f, space * 9.0f));
    // crossbar at the pitch
    const Clef clef = score->clefAtEntry (c.staff, c.measure, c.entry);
    const float y = yForPos (c.staff, staffPosition (c.dia, clef));
    g.fillRect (juce::Rectangle<float> (x - space * 1.4f, y - 1.0f, space * 2.8f, 2.0f));

    // mode text in the frame
    g.setColour (theme.frame);
    g.setFont (juce::FontOptions (space * 1.1f));
    juce::String mode = editor->insertMode() ? "INSERT" : "";
    if (editor->pendingTuplet().active())
        mode << (mode.isEmpty() ? "" : "  ") << editor->pendingTuplet().actual << ":" << editor->pendingTuplet().normal;
    g.drawText (mode, juce::Rectangle<float> (mg.x + space * 0.4f, mid ((int) geo.middleY.size() - 1) + space * 4.2f, mg.width, space * 1.6f),
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
            const auto& tx = geo.measures[(size_t) m].tickX;
            float x = geo.measures[(size_t) m].firstNoteX;
            for (size_t k = 0; k + 1 < tx.size(); ++k)
                if (t >= (double) tx[k].first && t <= (double) tx[k + 1].first)
                {
                    const double f = (t - (double) tx[k].first) / std::max<double> (1.0, (double) (tx[k + 1].first - tx[k].first));
                    x = tx[k].second + (float) f * (tx[k + 1].second - tx[k].second);
                    break;
                }
            useMeasure (m);
            g.setColour (theme.playhead);
            g.fillRect (juce::Rectangle<float> (x, mid (0) - space * 4, 2.0f, mid ((int) geo.middleY.size() - 1) - mid (0) + space * 8));
            return;
        }
        t -= (double) cap;
    }
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
        const juce::String us (")!@#$%^&*("), de ("=!\"\xc2\xa7$%&/()");
        int i = us.indexOfChar (c);
        if (i >= 0) return '0' + i;
        i = juce::String (juce::CharPointer_UTF8 ("=!\"\xc2\xa7$%&/()")).indexOfChar (c);
        if (i >= 0) return '0' + i;
        (void) de;
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
        if (b.command == Command::TupletDialog)
        {
            showTupletDialog();
            return true;
        }
        if (b.command == Command::MeasureMenu)
        {
            computeLayout();
            const auto& mg = geo.measures[(size_t) editor->cursor().measure];
            showMeasureMenu (editor->cursor().measure,
                             juce::Rectangle<float> (mg.x - scrollX, 0, mg.width, (float) getHeight()).toNearestInt());
            return true;
        }
        editor->perform (b);
        repaint();
        return true; // bound keys are always consumed, even when nothing changed
    }
    if (onKey) onKey (describeKey (candidates.front()), {});
    return false; // unbound: let the host have it (e.g. space for play)
}

bool StaffView::hitTest (juce::Point<float> p, int& staff, int& measure, int& entry, int& dia) const
{
    if (! score || geo.measures.empty()) return false;
    const float x = p.x + scrollX;
    staff = 0;
    float best = 1e9f;
    for (int s = 0; s < (int) geo.middleY.size(); ++s)
        if (std::abs (p.y - mid (s)) < best) { best = std::abs (p.y - mid (s)); staff = s; }
    measure = -1;
    for (int m = 0; m < (int) geo.measures.size(); ++m)
        if (x >= geo.measures[(size_t) m].x && x < geo.measures[(size_t) m].x + geo.measures[(size_t) m].width) measure = m;
    if (measure < 0) return false;
    const auto& egs = geo.measures[(size_t) measure].entries[(size_t) staff];
    entry = (int) egs.size();
    for (int i = 0; i < (int) egs.size(); ++i)
        if (x < egs[(size_t) i].x + headWidth() + space * 0.6f) { entry = i; break; }
    const int pos = juce::roundToInt ((mid (staff) - p.y) / (space * 0.5f));
    dia = pos + clefMiddleLine (score->clefAtEntry (staff, measure, entry));
    return true;
}

void StaffView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    int staff, measure, entry, dia;
    if (! hitTest (e.position, staff, measure, entry, dia)) return;
    if (e.mods.isPopupMenu())
    {
        const auto& mg = geo.measures[(size_t) measure];
        showMeasureMenu (measure, juce::Rectangle<float> (mg.x - scrollX, 0, mg.width, (float) getHeight()).toNearestInt());
        return;
    }
    if (editor)
    {
        followCursor = false;
        editor->setCursor ({ staff, measure, entry, dia });
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
    const float d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * space * 40.0f;
    scrollX = juce::jlimit (0.0f, std::max (0.0f, geo.totalWidth - (float) getWidth()), scrollX - d);
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
    const bool hasEntry = entry < (int) score->staves[(size_t) staff].measures[(size_t) m].entries.size();
    for (int c = 0; c < (int) Clef::Count; ++c)
    {
        const Clef cl = (Clef) c;
        clefHere.addItem (clefName (cl), hasEntry, false, act ([m, staff, entry, cl] (Score& s) {
            s.staves[(size_t) staff].measures[(size_t) m].entries[(size_t) entry].clefBefore = cl;
        }));
    }
    if (hasEntry && score->staves[(size_t) staff].measures[(size_t) m].entries[(size_t) entry].clefBefore)
        clefHere.addItem ("Remove clef change here", act ([m, staff, entry] (Score& s) {
            s.staves[(size_t) staff].measures[(size_t) m].entries[(size_t) entry].clefBefore.reset();
        }));
    if (m > 0 && score->staves[(size_t) staff].measures[(size_t) m].clefChange)
        clefStart.addItem ("Remove clef change", act ([m, staff] (Score& s) { s.staves[(size_t) staff].measures[(size_t) m].clefChange.reset(); }));
    menu.addSubMenu ("Clef at start of measure", clefStart);
    menu.addSubMenu ("Clef before the note at the cursor", clefHere, hasEntry);

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
